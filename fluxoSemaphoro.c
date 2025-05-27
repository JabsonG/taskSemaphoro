// Inclusão das bibliotecas padrão da Raspberry Pi Pico e periféricos
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"
#include "lib/ssd1306.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "buzzer.h"
#include "pico/bootrom.h"
#include "stdio.h"

// Definições de hardware (pinos e endereços)
#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define ENDERECO 0x3C
#define BOTAO_ENTRADA 5 // Gera evento de entrada
#define BOTAO_SAIDA 6   // BOOTSEL
#define BOTAO_RESET 22  // Botão do Joystick para reset
#define PINO_LED_VERMELHO 13
#define PINO_LED_AZUL 12
#define PINO_LED_VERDE 11
#define PINO_BUZZER 10

#define MAX_PESSOAS 5
#define VALOR_INICIAL_CONTADOR 0

// Variáveis Globais
ssd1306_t ssd;                         // Estrutura de controle do display OLED
bool modo_cor_fundo = true;           // Modo de fundo para o display
char msg_display[35];                 // Buffer de texto para exibir no display
absolute_time_t debounceA = 0;        // Controle de debounce para botão A
absolute_time_t debounceB = 0;        // Controle de debounce para botão B
absolute_time_t debounceReset = 0;    // Controle de debounce para botão reset

// Semáforos e mutexes
SemaphoreHandle_t semContador;            // Semáforo de contagem de pessoas
SemaphoreHandle_t semReset;               // Sinaliza pedido de reset
SemaphoreHandle_t mutexDisplay;           // Garante acesso exclusivo ao display
SemaphoreHandle_t semEntradaDetectada;    // Evento de entrada detectado
SemaphoreHandle_t semSaidaDetectada;      // Evento de saída detectado
SemaphoreHandle_t semLotadoBuzz;          // Sinaliza buzzer por local cheio
SemaphoreHandle_t mutexTotalPessoas;      // Protege variável totalPessoas

volatile uint16_t totalPessoas = 0;       // Contador de pessoas no local

// Atualiza LEDs conforme a ocupação atual
void vTaskLED(void *params){
    uint16_t pessoasAtual;

    while(true){
        if (xSemaphoreTake(mutexTotalPessoas, portMAX_DELAY) == pdTRUE) {
            pessoasAtual = totalPessoas;
            xSemaphoreGive(mutexTotalPessoas);
        } else {
            pessoasAtual = 0;
        }

        if(pessoasAtual == 0){
            gpio_put(PINO_LED_VERMELHO, false);
            gpio_put(PINO_LED_VERDE, false);
            gpio_put(PINO_LED_AZUL, true);

        } else if(pessoasAtual < MAX_PESSOAS - 1){
            gpio_put(PINO_LED_VERMELHO, false);
            gpio_put(PINO_LED_VERDE, true);
            gpio_put(PINO_LED_AZUL, false);

        } else if(pessoasAtual == MAX_PESSOAS - 1){
            gpio_put(PINO_LED_VERMELHO, true);
            gpio_put(PINO_LED_VERDE, true);
            gpio_put(PINO_LED_AZUL, false);

        } else if(pessoasAtual == MAX_PESSOAS){
            gpio_put(PINO_LED_VERMELHO, true);
            gpio_put(PINO_LED_VERDE, false);
            gpio_put(PINO_LED_AZUL, false);
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// Task que lida com eventos de entrada
void vTaskEntrada(void *params){
    bool mostrarEntrada = false;
    const TickType_t tempoExibicao = pdMS_TO_TICKS(500);

    while (true) {
        mostrarEntrada = false;

        if (xSemaphoreTake(semEntradaDetectada, 0) == pdTRUE) {
            if (xSemaphoreTake(mutexTotalPessoas, portMAX_DELAY) == pdTRUE) {
                if (totalPessoas < MAX_PESSOAS) {
                    totalPessoas++;
                    if (semContador != NULL) xSemaphoreGive(semContador);
                    mostrarEntrada = true;
                } else {
                    if (semLotadoBuzz != NULL) xSemaphoreGive(semLotadoBuzz);
                }
                xSemaphoreGive(mutexTotalPessoas);
            }
        }

        if (xSemaphoreTake(mutexDisplay, portMAX_DELAY) == pdTRUE) { 
            ssd1306_fill(&ssd, !modo_cor_fundo);
            uint16_t pessoasParaDisplay; 
            
            if (xSemaphoreTake(mutexTotalPessoas, portMAX_DELAY) == pdTRUE) {
                pessoasParaDisplay = totalPessoas;
                xSemaphoreGive(mutexTotalPessoas);
            } else {
                pessoasParaDisplay = 0;
            }

            if (mostrarEntrada) {
                ssd1306_draw_string(&ssd, "Pessoa(s) ", centralizar_texto("Pessoa(s) "), 20);
                ssd1306_draw_string(&ssd, "Entraram", centralizar_texto("Entraram"), 30);
                ssd1306_draw_string(&ssd, "No local", centralizar_texto("No local"), 40);
                ssd1306_send_data(&ssd);
                vTaskDelay(tempoExibicao); 
            } else {
                if (pessoasParaDisplay == 0) {
                    ssd1306_draw_string(&ssd, "No aguardo", centralizar_texto("No aguardo"), 20);
                    ssd1306_draw_string(&ssd, "...", centralizar_texto("..."), 40);
                } else {
                    ssd1306_draw_string(&ssd, "N. de pessoas", centralizar_texto("N. de pessoas"), 20);
                    snprintf(msg_display, sizeof(msg_display), "No Local: %u", pessoasParaDisplay);
                    ssd1306_draw_string(&ssd, msg_display, centralizar_texto(msg_display), 40);
                }
                ssd1306_send_data(&ssd);
            }
            xSemaphoreGive(mutexDisplay);
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// Task que lida com eventos de saída
void vTaskSaida(void *params){
    const TickType_t tempoExibicao = pdMS_TO_TICKS(500);

    while (true) {
        if (xSemaphoreTake(semSaidaDetectada, portMAX_DELAY) == pdTRUE) {
            bool houveSaida = false;
            if (xSemaphoreTake(mutexTotalPessoas, portMAX_DELAY) == pdTRUE) {
                if (totalPessoas > 0) {
                    totalPessoas--;
                    houveSaida = true;
                    if (semContador != NULL) xSemaphoreTake(semContador, 0);
                }
                xSemaphoreGive(mutexTotalPessoas);
            }

            if (houveSaida) {
                if (xSemaphoreTake(mutexDisplay, portMAX_DELAY) == pdTRUE) {
                    ssd1306_fill(&ssd, !modo_cor_fundo);
                    ssd1306_draw_string(&ssd, "Pessoa(s) ", centralizar_texto("Pessoa(s) "), 20);
                    ssd1306_draw_string(&ssd, "Sairam", centralizar_texto("Sairam"), 30);
                    ssd1306_draw_string(&ssd, "Do recinto!", centralizar_texto("Do recinto!"), 40);
                    ssd1306_send_data(&ssd);
                    vTaskDelay(tempoExibicao);
                    xSemaphoreGive(mutexDisplay);
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// Task responsável por resetar a contagem
void vTaskReset(void *params){
    BaseType_t resultado;

    while(true){
        if(xSemaphoreTake(semReset, portMAX_DELAY) == pdTRUE){
           
            if (xSemaphoreTake(mutexTotalPessoas, portMAX_DELAY) == pdTRUE) {
                totalPessoas = 0;
                xSemaphoreGive(mutexTotalPessoas);
            }

            do {
                resultado = xSemaphoreTake(semContador, 0);
            } while(resultado == pdTRUE);

            if(xSemaphoreTake(mutexDisplay, portMAX_DELAY) == pdTRUE){
                ssd1306_fill(&ssd, !modo_cor_fundo);
                ssd1306_draw_string(&ssd, "Reset", centralizar_texto("Reset"), 20);
                ssd1306_draw_string(&ssd, "Concluido!", centralizar_texto("Concluido!"), 40);
                ssd1306_send_data(&ssd);

                buzz(PINO_BUZZER, 600, 200);
                vTaskDelay(pdMS_TO_TICKS(250));
                buzz(PINO_BUZZER, 600, 200);

                xSemaphoreGive(mutexDisplay);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// Task que toca o buzzer quando o local está cheio
void vTaskBuzzLotado(void *pvParameters) {
    while (true) {
        if (xSemaphoreTake(semLotadoBuzz, portMAX_DELAY) == pdTRUE) {
            buzz(PINO_BUZZER, 750, 150); 
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// Handler de interrupção dos botões com debounce por software
void gpio_irq_handler(uint gpio, uint32_t events)
{   
    BaseType_t taskAwoken = pdFALSE;
    absolute_time_t agora = to_ms_since_boot(get_absolute_time());

    if (gpio == BOTAO_ENTRADA && agora - debounceA > 200) {
        if(semEntradaDetectada != NULL)
            xSemaphoreGiveFromISR(semEntradaDetectada, &taskAwoken); 
        debounceA = agora;
    }
    else if (gpio == BOTAO_SAIDA && totalPessoas > 0 && agora - debounceB > 200) {
        if(semSaidaDetectada != NULL)
            xSemaphoreGiveFromISR(semSaidaDetectada, &taskAwoken);
        debounceB = agora;
    }
    else if(gpio == BOTAO_RESET && agora - debounceReset > 200) {
        if(semReset != NULL)
            xSemaphoreGiveFromISR(semReset, &taskAwoken);
        debounceReset = agora;
    }

    portYIELD_FROM_ISR(taskAwoken);   
}

// Inicialização geral do sistema
void setup(){
    // Inicializa o I2C e o display
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, ENDERECO, I2C_PORT);
    ssd1306_config(&ssd);
    ssd1306_send_data(&ssd);

    // Inicializa LEDs RGB
    gpio_init(PINO_LED_VERMELHO); gpio_set_dir(PINO_LED_VERMELHO, GPIO_OUT); gpio_put(PINO_LED_VERMELHO, false);
    gpio_init(PINO_LED_AZUL); gpio_set_dir(PINO_LED_AZUL, GPIO_OUT); gpio_put(PINO_LED_AZUL, false);
    gpio_init(PINO_LED_VERDE); gpio_set_dir(PINO_LED_VERDE, GPIO_OUT); gpio_put(PINO_LED_VERDE, false);

    // Inicializa botões com interrupções
    gpio_init(BOTAO_ENTRADA); gpio_set_dir(BOTAO_ENTRADA, GPIO_IN); gpio_pull_up(BOTAO_ENTRADA);
    gpio_set_irq_enabled_with_callback(BOTAO_ENTRADA, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);

    gpio_init(BOTAO_SAIDA); gpio_set_dir(BOTAO_SAIDA, GPIO_IN); gpio_pull_up(BOTAO_SAIDA);
    gpio_set_irq_enabled_with_callback(BOTAO_SAIDA, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);

    gpio_init(BOTAO_RESET); gpio_set_dir(BOTAO_RESET, GPIO_IN); gpio_pull_up(BOTAO_RESET);
    gpio_set_irq_enabled_with_callback(BOTAO_RESET, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);

    // Inicializa o buzzer
    gpio_init(PINO_BUZZER); gpio_set_dir(PINO_BUZZER, GPIO_OUT);
}

int main()
{
    setup();
    stdio_init_all();
    
    // Criação de mutexes e semáforos
    mutexDisplay = xSemaphoreCreateMutex();    
    mutexTotalPessoas = xSemaphoreCreateMutex(); 
    semContador = xSemaphoreCreateCounting(MAX_PESSOAS, VALOR_INICIAL_CONTADOR);
    semReset = xSemaphoreCreateBinary();
    semEntradaDetectada = xSemaphoreCreateBinary();
    semSaidaDetectada = xSemaphoreCreateBinary();
    semLotadoBuzz = xSemaphoreCreateBinary();

    // Criação das tarefas do sistema
    xTaskCreate(vTaskEntrada, "Task de Entrada", configMINIMAL_STACK_SIZE + 128, NULL, 1, NULL);
    xTaskCreate(vTaskSaida, "Task de Saida", configMINIMAL_STACK_SIZE + 128, NULL, 1, NULL);
    xTaskCreate(vTaskReset, "Task de Reset", configMINIMAL_STACK_SIZE + 128, NULL, 2, NULL);
    xTaskCreate(vTaskLED, "Task de LED", configMINIMAL_STACK_SIZE + 128, NULL, 1, NULL);
    xTaskCreate(vTaskBuzzLotado, "Task do Buzzer", configMINIMAL_STACK_SIZE + 128, NULL, 3, NULL);

    vTaskStartScheduler(); // Inicia o agendador do FreeRTOS

    panic_unsupported(); // Caso o agendador termine, exibe erro
}
