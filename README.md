# Sistema de Controle de Entrada e Saída com Raspberry Pi Pico W

Este projeto implementa um sistema inteligente de controle de entrada e saída de pessoas usando uma **Raspberry Pi Pico W**, com **FreeRTOS**, display **OLED SSD1306**, LEDs RGB, botões físicos e **buzzer**.

## Funcionalidades

- Contagem de pessoas no ambiente em tempo real
- Exibição de mensagens no display OLED
- Sinalização visual com LEDs:
  - Azul: ambiente vazio
  - Verde: ambiente com pessoas
  - Amarelo: ambiente quase lotado
  - Vermelho: ambiente lotado
- Sinal sonoro com buzzer quando o limite máximo é atingido
- Reset manual da contagem com feedback sonoro e visual
- Interface responsiva com tratamento de **debounce** via software
- Gerenciado com **FreeRTOS** utilizando tarefas, semáforos e mutexes

## Hardware Utilizado

- Raspberry Pi Pico W
- Display OLED SSD1306 (via I2C - pinos 14 e 15)
- LED RGB (pinos 11, 12 e 13)
- Buzzer (pino 10)
- Botão de entrada (pino 5)
- Botão de saída (pino 6 - BOOTSEL)
- Botão de reset (pino 22 - joystick)
- Resistores de pull-up nos botões (ativados via software)

## Pinos e Conexões

| Componente    | Pino da Pico |
|---------------|--------------|
| SDA (I2C)     | GP14         |
| SCL (I2C)     | GP15         |
| LED Vermelho  | GP13         |
| LED Azul      | GP12         |
| LED Verde     | GP11         |
| Buzzer        | GP10         |
| Botão Entrada | GP5          |
| Botão Saída   | GP6          |
| Botão Reset   | GP22         |

## Como Funciona

- Ao pressionar o botão de **entrada**, uma pessoa é adicionada ao contador, se o ambiente não estiver lotado.
- Ao pressionar o botão de **saída**, uma pessoa é removida do contador.
- O botão de **reset** zera a contagem e toca o buzzer duas vezes.
- O display mostra mensagens de entrada, saída, ou número atual de pessoas.
- O buzzer emite um som sempre que o ambiente atinge a lotação máxima (10 pessoas).
- O LED RGB muda de cor conforme o estado de ocupação.

## Requisitos de Software

- SDK da Raspberry Pi Pico
- Biblioteca SSD1306 (`ssd1306.h`, `ssd1306.c`)
- Biblioteca `buzzer.h`
- FreeRTOS (port para RP2040)
- `CMake`, `GCC` ou VS Code configurado com toolchain para Pico

## Organização do Código

- `main.c`: Arquivo principal com inicialização e tarefas do FreeRTOS
- `buzzer.c / buzzer.h`: Funções auxiliares para tocar o buzzer
- `ssd1306.c / ssd1306.h`: Interface de controle do display OLED
- `CMakeLists.txt`: Arquivo de build

## Tarefas FreeRTOS

| Tarefa           | Descrição                             | Prioridade |
|------------------|----------------------------------------|------------|
| vTaskEntrada     | Gerencia eventos de entrada            | 1          |
| vTaskSaida       | Gerencia eventos de saída              | 1          |
| vTaskReset       | Zera a contagem                        | 2          |
| vTaskLED         | Atualiza estado dos LEDs RGB           | 1          |
| vTaskBuzzLotado  | Emite som se ambiente estiver lotado   | 3          |
