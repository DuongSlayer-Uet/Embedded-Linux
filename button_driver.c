#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

#define DEVICE_NAME "button_dev"

static dev_t dev_num;
static struct cdev my_cdev;
static struct class *my_class;

#define MAX_BUTTONS 2
#define DEBOUNCE_MS 2000

struct my_button {
    int gpio;
    int irq;
    char name[20];
    unsigned long last_jiffies;
};

static struct my_button buttons[MAX_BUTTONS];
static int num_buttons = 0;

extern void led_toggle_by_name(const char *name);

static ssize_t my_read(struct file *file, char __user *buf,
                        size_t len, loff_t *off)
{
    char kbuf[64];
    int i;
    int pos = 0;
    int value;

    if (*off > 0)
        return 0;

    for (i = 0; i < num_buttons; i++) {

        value = gpio_get_value(buttons[i].gpio);

        pos += scnprintf(kbuf + pos, sizeof(kbuf) - pos,
                         "%s=%d ",
                         buttons[i].name,
                         value);
    }

    pos += scnprintf(kbuf + pos, sizeof(kbuf) - pos, "\n");

    if (copy_to_user(buf, kbuf, pos))
        return -EFAULT;

    *off = pos;
    return pos;
}

static const struct file_operations fops = {
    .owner = THIS_MODULE,
    .read  = my_read,
};

static irqreturn_t button_isr(int irq, void *dev_id)
{
    struct my_button *btn = dev_id;

    unsigned long now = jiffies;

    // nếu chưa đủ thời gian thì bỏ qua
    if (time_before(now, btn->last_jiffies + msecs_to_jiffies(DEBOUNCE_MS))) {
        return IRQ_HANDLED;
    }

    btn->last_jiffies = now;

    pr_info("Button %s pressed!\n", btn->name);

    if (strcmp(btn->name, "BTN1") == 0) {
        led_toggle_by_name("led1");
    } 
    else if (strcmp(btn->name, "BTN2") == 0) {
        led_toggle_by_name("led2");
    }

    return IRQ_HANDLED;
}

static int my_probe(struct platform_device *pdev)
{
    struct device_node *child;
    int i = 0;
    int ret;

    pr_info("GPIO Button Driver Probe\n");

    for_each_child_of_node(pdev->dev.of_node, child) {

        struct my_button *btn = &buttons[i];
        const char *label;

        // Get GPIO
        btn->gpio = of_get_named_gpio(child, "gpios", 0);
        if (!gpio_is_valid(btn->gpio)) {
            pr_err("Invalid GPIO\n");
            return -EINVAL;
        }

        // Request GPIO
        ret = gpio_request(btn->gpio, "button_gpio");
        if (ret) {
            pr_err("Failed to request GPIO\n");
            return ret;
        }

        gpio_direction_input(btn->gpio);

        // Convert GPIO → IRQ
        btn->irq = gpio_to_irq(btn->gpio);
        if (btn->irq < 0) {
            pr_err("Failed to get IRQ\n");
            return btn->irq;
        }

        // Get label
        if (of_property_read_string(child, "label", &label) == 0) {
            snprintf(btn->name, sizeof(btn->name), "%s", label);
        } else {
            snprintf(btn->name, sizeof(btn->name), "button%d", i);
        }
        
        // Request IRQ 
        // param: INT num, handler func, trigger type, name, input argument var for handler func
        ret = request_irq(btn->irq,
                          button_isr,
                          IRQF_TRIGGER_FALLING,
                          btn->name,
                          btn);
        if (ret) {
            pr_err("Failed to request IRQ\n");
            gpio_free(btn->gpio);
            return ret;
        }

        pr_info("Registered %s: GPIO=%d, IRQ=%d\n",
                btn->name, btn->gpio, btn->irq);
        i++;
    }

    num_buttons = i;

    ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
    if (ret)
        return ret;

    cdev_init(&my_cdev, &fops);
    ret = cdev_add(&my_cdev, dev_num, 1);
    if (ret)
        return ret;

    my_class = class_create(THIS_MODULE, DEVICE_NAME);
    device_create(my_class, NULL, dev_num, NULL, DEVICE_NAME);

    pr_info("Device created: /dev/%s\n", DEVICE_NAME);
    return 0;
}

static int my_remove(struct platform_device *pdev)
{
    int i;

    device_destroy(my_class, dev_num);
    class_destroy(my_class);
    cdev_del(&my_cdev);
    unregister_chrdev_region(dev_num, 1);

    for (i = 0; i < num_buttons; i++) {
        free_irq(buttons[i].irq, &buttons[i]);
        gpio_free(buttons[i].gpio);
    }

    pr_info("GPIO Button Driver Removed\n");
    return 0;
}

static const struct of_device_id my_of_match[] = {
    { .compatible = "my,gpio-button" },
    { }
};

MODULE_DEVICE_TABLE(of, my_of_match);

static struct platform_driver my_driver = {
    .probe  = my_probe,
    .remove = my_remove,
    .driver = {
        .name = "my_gpio_button_driver",
        .of_match_table = my_of_match,
    },
};

module_platform_driver(my_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Duong");
MODULE_DESCRIPTION("GPIO Button Driver with IRQ");