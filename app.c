#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#define LED_DEV "/dev/led_dev"
#define BUTTON_DEV "/dev/button_dev"

int main() {
    char input[100];

    while (1) {
        printf("Enter command: ");
        fgets(input, sizeof(input), stdin);

        // remove newline
        input[strcspn(input, "\n")] = 0;

        // ===== LED COMMAND =====
        int led;
        char state[10];

        if (sscanf(input, "LED%d %s", &led, state) == 2) {
            if ((led == 1 || led == 2) &&
                (strcmp(state, "ON") == 0 || strcmp(state, "OFF") == 0)) {

                int fd = open(LED_DEV, O_WRONLY);
                if (fd < 0) {
                    perror("Open LED device failed");
                    continue;
                }

                write(fd, input, strlen(input));
                close(fd);

                printf("Sent to LED device: %s\n", input);
            } else {
                printf("Invalid LED command!\n");
            }
        }

        // ===== BUTTON COMMAND =====
        else if (strcmp(input, "READ BUTTON") == 0) {
            int fd = open(BUTTON_DEV, O_RDONLY);
            if (fd < 0) {
                perror("Open BUTTON device failed");
                continue;
            }

            char buffer[100];
            int n = read(fd, buffer, sizeof(buffer) - 1);
            if (n > 0) {
                buffer[n] = '\0';
                printf("Button state: %s\n", buffer);
            }

            close(fd);
        }

        // ===== INVALID =====
        else {
            printf("Invalid command!\n");
        }
    }

    return 0;
}