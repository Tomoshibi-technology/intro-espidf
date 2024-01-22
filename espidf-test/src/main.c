#include <stdio.h>
#include <driver/i2s_std.h>

void app_main(void)
{
		i2s_chan_config_t channel_config = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
		i2s_chan_handle_t tx_channel_handle;
		ESP_ERROR_CHECK(i2s_new_channel(&channel_config, &tx_channel_handle, NULL /* rx_handle */));

		i2s_std_config_t std_config = {
				.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(44100),
				.slot_cfg = I2S_STD_PHILIP_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
				.gpio_cfg = {
						.mclk = GPIO_NUM_0,
						.bclk = GPIO_NUM_25,
						.ws = GPIO_NUM_26,
						.dout = GPIO_NUM_27,
						.din = I2S_GPIO_UNUSED,
						.invert_flags = {
								.mclk_inv = false,
								.bclk_inv = false,
								.ws_inv = false
						}
				}
		};
		ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_channel_handle, &std_config));
		ESP_ERROR_CHECK(i2s_channel_enable(tx_channel_handle));

		const int SAMPLE_LENGTH = 4;
		int16_t* buffer = (int16_t*)calloc(sizeof(int16_t), SAMPLE_LENGTH);
		for (int i = 0; i < SAMPLE_LENGTH; ++i) {
				buffer[i] = (i & 1) ? 0x8000 + i : i;
		}
		for (;;) {
				size_t written_bytes = 0;
				const uint32_t TIMEOUT_MS = 1000; 
				ESP_ERROR_CHECK(i2s_channel_write(tx_channel_handle, buffer,
						SAMPLE_LENGTH * sizeof(int16_t), &written_bytes, TIMEOUT_MS));
		}
}
