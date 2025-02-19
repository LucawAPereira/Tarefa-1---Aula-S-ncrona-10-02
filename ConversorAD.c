#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "inc/ssd1306.h"
#include "inc/font.h"
#include "hardware/pwm.h"
#include "hardware/adc.h"
#include "pico/bootrom.h"


#define LEDg 11
#define LEDb 12
#define LEDr 13
#define BotaoA 5
#define BotaoB 6  // reset sem precisar apertar o bootsel
#define BotaoJS 22
#define VRY 27  // adc0
#define VRX 26  // adc1
#define wrap 4096 
#define div 40
#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define endereco 0x3C

int var=0; 
bool pwm_enabled=0;
bool cor = true;
bool centralizado = true;
uint conversao_x=0;  // parametros para definir limite das bordas x
uint conversao_y=0;   // parametros para definir limite das bordas y

uint slice_r;
uint slice_b;


void gpio_irq_handler(uint gpio, uint32_t events);

static volatile uint32_t last_time = 0;

int main()
{
    stdio_init_all();

    gpio_set_irq_enabled_with_callback(BotaoA, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    gpio_set_irq_enabled_with_callback(BotaoB, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    gpio_set_irq_enabled_with_callback(BotaoJS, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);

    gpio_set_function(LEDr, GPIO_FUNC_PWM);
    gpio_set_function(LEDb, GPIO_FUNC_PWM);

    slice_r = pwm_gpio_to_slice_num(LEDr);
    slice_b = pwm_gpio_to_slice_num(LEDb);

    pwm_set_clkdiv(slice_r, div);
    pwm_set_wrap(slice_r, wrap);
    pwm_set_enabled(slice_r, true);

    pwm_set_clkdiv(slice_b, div);
    pwm_set_wrap(slice_b, wrap);
    pwm_set_enabled(slice_b, true);

    gpio_init(LEDg);
    gpio_set_dir(LEDg, GPIO_OUT);
    gpio_put(LEDg, 0);

    gpio_init(BotaoA);
    gpio_set_dir(BotaoA, GPIO_IN);
    gpio_pull_up(BotaoA);
    gpio_init(BotaoJS);
    gpio_set_dir(BotaoJS, GPIO_IN);
    gpio_pull_up(BotaoJS);
    gpio_init(BotaoB);
    gpio_set_dir(BotaoB, GPIO_IN);
    gpio_pull_up(BotaoB);

    adc_init();
    adc_gpio_init(VRX);
    adc_gpio_init(VRY);

    // I2C Initialisation. Using it at 400Khz.
    i2c_init(I2C_PORT, 400 * 1000);

    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C); // Set the GPIO pin function to I2C
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C); // Set the GPIO pin function to I2C
    gpio_pull_up(I2C_SDA); // Pull up the data line
    gpio_pull_up(I2C_SCL); // Pull up the clock line
    ssd1306_t ssd; // Inicializa a estrutura do display
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, endereco, I2C_PORT); // Inicializa o display
    ssd1306_config(&ssd); // Configura o display
    ssd1306_send_data(&ssd);

    
    while (true)
    {
        if (centralizado == true) // comeca semprem com o quadrado no centro do display
        {
            ssd1306_fill(&ssd, !cor); // Limpa o display
            ssd1306_rect(&ssd, 3, 3, 122, 58, cor, !cor); // Desenha um retângulo 
            ssd1306_rect(&ssd, 28, 60, 8, 8, 1, 1);
            ssd1306_send_data(&ssd); // Atualiza o display
            centralizado = false;
            sleep_ms(20);
        }

        adc_select_input(0);
        uint16_t VRX_value = adc_read();
        adc_select_input(1);
        uint16_t VRY_value = adc_read();

        conversao_x = (VRX_value * 56) / 4096;  // definindo os limites das bordas display
        conversao_y = (VRY_value * 120) / 4096;  // definindo os limites das bordas display    

        ssd1306_fill(&ssd, !cor); // Limpa o display
        ssd1306_rect(&ssd, 3, 3, 122, 58, cor, !cor); // Desenha o retangulo maior
        ssd1306_rect(&ssd, conversao_x, conversao_y, 8, 8, 1, 1); // (&ssd, y, x, tam_y, tam_x, 1,1)
        ssd1306_send_data(&ssd); // Atualiza o display

        printf("adc x = %d\n", VRY_value); // - DEBUG
        printf("adc y = %d\n", VRX_value); // - DEBUG

        if(VRX_value >= 1900 && VRX_value <= 2200 && VRY_value >= 1900 && VRY_value <= 2200 )
        {
            pwm_set_gpio_level(LEDr, 0);
            pwm_set_gpio_level(LEDb, 0);
            ssd1306_fill(&ssd, !cor); // Limpa o display   
            ssd1306_rect(&ssd, 3, 3, 8, 8, 1, 1);
            ssd1306_rect(&ssd, 28, 60, 8, 8, 1, 1); // (, y, x, tam_y, tam_x, 1,1) mantem o quadrado no centro do display
        }
        if (VRY_value <= 1900 || VRY_value >= 2200) {
            pwm_set_gpio_level(LEDr, VRY_value);
        }
        if (VRX_value <= 1900 || VRX_value >= 2200){
            pwm_set_gpio_level(LEDb, VRX_value);
        }

        sleep_ms(10);
    }

}


void gpio_irq_handler(uint gpio, uint32_t events)
{
    uint32_t current_time = to_us_since_boot(get_absolute_time());

    if (current_time - last_time >= 350000)
    {
        last_time = current_time;

        // I2C Initialisation. Using it at 400Khz.
        i2c_init(I2C_PORT, 400 * 1000);

        gpio_set_function(I2C_SDA, GPIO_FUNC_I2C); // Set the GPIO pin function to I2C
        gpio_set_function(I2C_SCL, GPIO_FUNC_I2C); // Set the GPIO pin function to I2C
        gpio_pull_up(I2C_SDA); // Pull up the data line
        gpio_pull_up(I2C_SCL); // Pull up the clock line
        ssd1306_t ssd; // Inicializa a estrutura do display
        ssd1306_init(&ssd, WIDTH, HEIGHT, false, endereco, I2C_PORT); // Inicializa o display
        ssd1306_config(&ssd); // Configura o display
        ssd1306_send_data(&ssd);

        if(gpio == BotaoJS)
        {
            cor = !cor;
            gpio_put(LEDg,!gpio_get(LEDg));
        }

        if(gpio == BotaoA)
        {
            pwm_enabled = !pwm_enabled; // Alterna o estado do PWM  
            
            if (pwm_enabled) {  
                // Ativa o PWM  
                pwm_set_enabled(slice_r, true);
                pwm_set_enabled(slice_b, true);

            } else {  
                // Desativa o PWM  
                pwm_set_enabled(slice_r, false);
                pwm_set_enabled(slice_b, false);
             } 

        }
        if (gpio == BotaoB)
        {
            reset_usb_boot(0,0);
        }
    }
    
}