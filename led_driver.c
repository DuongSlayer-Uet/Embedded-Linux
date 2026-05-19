#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>

#define DEV_NAME "led_dev"
#define MAX_BUF 64

struct my_led
{
    int gpio;
    char name[20];
};

static dev_t dev_num;
static struct cdev my_cdev;
static struct class *my_class;
static struct device *my_device;

static struct my_led *leds;
static int led_count;

static int my_open(struct inode *inode, struct file *file)
{
    pr_info("LED device opened\n");
    return 0;
}

static ssize_t my_write(struct file *file,
                        const char __user *buf,
                        size_t len,
                        loff_t *off)
{
    char kbuf[MAX_BUF];
    int i;
    char cmd_led[20];
    char cmd_action[10];

    if (len > MAX_BUF - 1)
        len = MAX_BUF - 1;

    if (copy_from_user(kbuf, buf, len))
        return -EFAULT;

    kbuf[len] = '\0';

    // parse command, VD: "LED1","ON"
    sscanf(kbuf, "%s %s", cmd_led, cmd_action);
    

    for (i = 0; i < led_count; i++) {
        // reference to devicetree label
        if (strcasecmp(leds[i].name, cmd_led) == 0) {
            
            if (strcasecmp(cmd_action, "ON") == 0)
                gpio_set_value(leds[i].gpio, 1);
            else if (strcasecmp(cmd_action, "OFF") == 0)
                gpio_set_value(leds[i].gpio, 0);

            break;
        }
    }
    pr_info("Unknown cmd: %s\n", kbuf);
    return len;
}

static int my_release(struct inode *inode, struct file *file)
{
    pr_info("LED device closed\n");
    return 0;
}

static const struct file_operations my_fops = {
    .owner = THIS_MODULE,
    .open = my_open,
    .release = my_release,
    .write = my_write,
};

void led_toggle_by_name(const char *name)
{
    int i;

    for (i = 0; i < led_count; i++) {
        if (strcasecmp(leds[i].name, name) == 0) {
            int val = gpio_get_value(leds[i].gpio);
            gpio_set_value(leds[i].gpio, !val);
            pr_info("Toggled %s\n", name);
            return;
        }
    }

    pr_warn("LED %s not found\n", name);
}
EXPORT_SYMBOL(led_toggle_by_name);

static int my_probe(struct platform_device *pdev)
{
    struct device_node *child;
    int i = 0;
    int ret;
    // device tree
    led_count = of_get_child_count(pdev->dev.of_node);
    if (!led_count)
        return -EINVAL;

    leds = kzalloc(sizeof(struct my_led) * led_count, GFP_KERNEL);
    if (!leds)
        return -ENOMEM;

    for_each_child_of_node(pdev->dev.of_node, child)
    {
        struct my_led *led = &leds[i];
        const char *label;

        led->gpio = of_get_named_gpio(child, "gpios", 0);
        if (!gpio_is_valid(led->gpio))
        {
            ret = -EINVAL;
            goto err_leds;
        }

        if (of_property_read_string(child, "label", &label) == 0)
            snprintf(led->name, sizeof(led->name), "%s", label);
        else
            snprintf(led->name, sizeof(led->name), "led%d", i);

        ret = gpio_request(led->gpio, led->name);
        if (ret)
            goto err_leds;

        ret = gpio_direction_output(led->gpio, 0);
        if (ret)
            goto err_leds;

        i++;
    }
    // device file
    ret = alloc_chrdev_region(&dev_num, 0, 1, DEV_NAME);
    if (ret)
        goto err_leds;

    cdev_init(&my_cdev, &my_fops);
    ret = cdev_add(&my_cdev, dev_num, 1);
    if (ret)
        goto err_cdev;

    my_class = class_create(THIS_MODULE, DEV_NAME);
    if (IS_ERR(my_class))
    {
        ret = PTR_ERR(my_class);
        goto err_class;
    }

    my_device = device_create(my_class, NULL, dev_num, NULL, DEV_NAME);
    if (IS_ERR(my_device))
    {
        ret = PTR_ERR(my_device);
        goto err_device;
    }

    pr_info("Device /dev/%s created\n", DEV_NAME);
    return 0;

err_device:
    class_destroy(my_class);

err_class:
    cdev_del(&my_cdev);

err_cdev:
    unregister_chrdev_region(dev_num, 1);

err_leds:
{
    int j;
    for (j = 0; j < i; j++)
    {
        gpio_set_value(leds[j].gpio, 0);
        gpio_free(leds[j].gpio);
    }

    kfree(leds);
    return ret;
}
}

static int my_remove(struct platform_device *pdev)
{
    int i;

    if (leds)
    {
        for (i = 0; i < led_count; i++)
        {
            gpio_set_value(leds[i].gpio, 0);
            gpio_free(leds[i].gpio);
        }
        kfree(leds);
    }

    device_destroy(my_class, dev_num);
    class_destroy(my_class);
    cdev_del(&my_cdev);
    unregister_chrdev_region(dev_num, 1);

    pr_info("Driver removed\n");
    return 0;
}

static const struct of_device_id my_of_match[] = {
    {.compatible = "my,gpio-led"},
    {}};
MODULE_DEVICE_TABLE(of, my_of_match);

static struct platform_driver my_driver = {
    .probe = my_probe,
    .remove = my_remove,
    .driver = {
        .name = "my_gpio_led_driver",
        .of_match_table = my_of_match,
    },
};

module_platform_driver(my_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Duong");
MODULE_DESCRIPTION("Simple GPIO LED Driver");
