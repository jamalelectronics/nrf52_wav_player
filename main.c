// This code shows a small demo to play 16Bit HD WAV audio over I2S using nRF52

#include <stdio.h>
#include "nrf_drv_i2s.h"
#include "nrf_delay.h"
#include "app_util_platform.h"
#include "app_error.h"
#include "boards.h"
#include "nrf_gpio.h"

#include "audio16.h" // Include audio data

// Define the number of words per I2S data block
#define I2S_DATA_BLOCK_WORDS 2048

// I2S pin definitions
#define I2S_LRC_PIN 26
#define I2S_DOUT_PIN 27
#define I2S_CLK_PIN 25

// Button pin definition
#define BUTTON_PIN 8

// Define a double buffer for I2S transmission
static uint32_t m_buffer_tx[2][I2S_DATA_BLOCK_WORDS];

// Pointer to track the buffer block to be filled
static uint32_t *volatile mp_block_to_fill = NULL;

// Track the current position in the WAV file
uint32_t sound_index = 44;
uint32_t current_wav_size = 0;
uint8_t buffer_index = 0;
bool last_block_sent = false;

// Function to prepare data for playback
static void prepare_tx_data(uint32_t *p_block)
{
    uint16_t i;
    for (i = 0; i < I2S_DATA_BLOCK_WORDS; i++)
    {
        if (sound_index < current_wav_size)
        {
            // Retrieve 16-bit audio samples from AUDIO16 array
            uint32_t sample_l = (AUDIO16[(sound_index * 2) + 1] << 8) | AUDIO16[(sound_index * 2) + 0];
            p_block[i] = sample_l & 0x0000FFFF;
            sound_index++;
        }
        else
        {
            // Fill remaining buffer space with silence
            p_block[i] = 0x00000000;
        }
    }
}

// I2S event handler function
static void data_handler(nrf_drv_i2s_buffers_t const *p_released, uint32_t status)
{
    // Ensure the event is valid
    if (!(status & NRFX_I2S_STATUS_NEXT_BUFFERS_NEEDED))
    {
        return;
    }

    // If the last block has been sent, stop further processing
    if (sound_index >= current_wav_size && !last_block_sent)
    {
        last_block_sent = true;
        return;
    }

    // Prepare the next buffer for playback
    if (p_released->p_tx_buffer)
    {
        mp_block_to_fill = (uint32_t *)p_released->p_tx_buffer;
        prepare_tx_data(mp_block_to_fill);
    }

    // Switch to the next buffer
    buffer_index = (buffer_index + 1) % 2;
    nrf_drv_i2s_buffers_t next_buffers = {
        .p_tx_buffer = m_buffer_tx[buffer_index],
    };
    APP_ERROR_CHECK(nrf_drv_i2s_next_buffers_set(&next_buffers));
}

// Function to play a WAV file
void play_wav(const unsigned char *data, uint32_t wav_length)
{
    mp_block_to_fill = NULL;
    sound_index = 44; // WAV file header is 44 bytes, skip it
    current_wav_size = (wav_length - 44) / 2; // Convert byte length to sample count
    buffer_index = 0;
    last_block_sent = false;

    // Prepare the initial buffers with audio data
    prepare_tx_data(m_buffer_tx[0]);
    prepare_tx_data(m_buffer_tx[1]);

    // Start I2S transmission with the first buffer
    nrf_drv_i2s_buffers_t initial_buffers = {
        .p_tx_buffer = m_buffer_tx[0],
    };
    APP_ERROR_CHECK(nrf_drv_i2s_start(&initial_buffers, I2S_DATA_BLOCK_WORDS, 0));

    // Wait until the last audio block is played
    while (!last_block_sent)
    {
        __WFE();
        __SEV();
        __WFE();
    }

    // Stop I2S transmission
    nrf_drv_i2s_stop();
}

// Function to initialize I2S hardware
void init_i2s(void)
{
    nrf_drv_i2s_config_t config = NRF_DRV_I2S_DEFAULT_CONFIG;
    config.lrck_pin = I2S_LRC_PIN; // LRCK (Left-Right Clock) pin assignment
    config.sck_pin = I2S_CLK_PIN;  // Serial Clock (SCK) pin assignment
    config.sdout_pin = I2S_DOUT_PIN; // Serial Data Out (SDOUT) pin assignment
    config.sdin_pin = NRFX_I2S_PIN_NOT_USED; // SDIN not used
    config.mode = NRF_I2S_MODE_MASTER; // I2S master mode
    config.mck_setup = NRF_I2S_MCK_32MDIV16; // Master Clock setup
    config.ratio = NRF_I2S_RATIO_128X; // LRCK/SCK ratio setup
    config.sample_width = NRF_I2S_SWIDTH_16BIT; // 16-bit audio samples
    config.channels = NRF_I2S_CHANNELS_LEFT; // Only left channel used
    config.irq_priority = 7; // Interrupt priority
    APP_ERROR_CHECK(nrf_drv_i2s_init(&config, data_handler));
}

// Function to initialize button
void init_button(void)
{
    nrf_gpio_cfg_input(BUTTON_PIN, NRF_GPIO_PIN_PULLUP); // Configure button as input with pull-up resistor
}

int main(void)
{
    // Initialize the I2S module
    init_i2s();
    
    // Initialize the button
    init_button();
    
    // Main loop to wait for button press
    while (1)
    {
        if (nrf_gpio_pin_read(BUTTON_PIN) == 0) // Check if button is pressed
        {
            play_wav(AUDIO16, sizeof(AUDIO16)); // Play sound
        }
    }
}
