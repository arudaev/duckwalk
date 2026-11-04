#include "speaker_pcm.h"

#include <furi.h>
#include <furi_hal.h>
#include <storage/storage.h>
#include <stm32wbxx_ll_dma.h>
#include <stm32wbxx_ll_dmamux.h>

#define PWM_CYCLES_PER_SAMPLE 8U
#define SPEAKER_DMA_CHANNEL   LL_DMA_CHANNEL_3

uint8_t speaker_pcm_volume(void) {
    /* NotificationSettings is private to the firmware. Read its saved v2
     * layout instead of reaching into the live notification service. */
    uint8_t settings[24];
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    bool opened = storage_file_open(
        file, INT_PATH(".notification.settings"), FSAM_READ, FSOM_OPEN_EXISTING);
    bool valid = opened && storage_file_size(file) == sizeof(settings) &&
                 storage_file_read(file, settings, sizeof(settings)) == sizeof(settings) &&
                 settings[0] == 2;
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    if(!opened) return 100; /* Firmware default on a fresh device. */
    if(!valid) return 0;
    float volume;
    memcpy(&volume, settings + 12, sizeof(volume));
    return volume >= 0.0f && volume <= 1.0f ? (uint8_t)(volume * 100.0f + 0.5f) : 0;
}

void speaker_pcm_play(const uint8_t* samples, size_t count, uint8_t volume) {
    if(!volume || furi_hal_rtc_is_flag_set(FuriHalRtcFlagStealthMode)) return;

    /* TIM16 drives the piezo at 64 kHz. Its repetition counter requests
     * one DMA transfer every eight PWM periods. DMA1 channel 3 is available
     * to apps on the official firmware. */
    const uint32_t period = SystemCoreClock / (SPEAKER_PCM_RATE * PWM_CYCLES_PER_SAMPLE);
    uint16_t* duty = malloc((count + 1) * sizeof(*duty));
    if(!duty) return;
    for(size_t i = 0; i < count; i++) {
        int32_t sample = (int32_t)samples[i] - 128;
        duty[i] = (int32_t)(period / 2) + sample * volume * (int32_t)period / 25600;
    }
    duty[count] = period / 2;

    if(!furi_hal_speaker_acquire(furi_ms_to_ticks(30))) {
        free(duty);
        return;
    }
    if(!LL_DMA_IsEnabledChannel(DMA1, SPEAKER_DMA_CHANNEL)) {
        furi_hal_bus_reset(FuriHalBusTIM16);
        TIM16->CR1 = TIM_CR1_ARPE | TIM_CR1_URS;
        TIM16->CCMR1 = TIM_CCMR1_OC1PE | (6U << TIM_CCMR1_OC1M_Pos);
        TIM16->ARR = period - 1;
        TIM16->RCR = PWM_CYCLES_PER_SAMPLE - 1;
        TIM16->CCR1 = period / 2;
        TIM16->EGR = TIM_EGR_UG;
        TIM16->CCER = TIM_CCER_CC1E;
        TIM16->BDTR = TIM_BDTR_MOE;

        LL_DMA_ClearFlag_GI3(DMA1);
        LL_DMA_SetPeriphRequest(DMA1, SPEAKER_DMA_CHANNEL, LL_DMAMUX_REQ_TIM16_UP);
        LL_DMA_ConfigTransfer(
            DMA1,
            SPEAKER_DMA_CHANNEL,
            LL_DMA_DIRECTION_MEMORY_TO_PERIPH | LL_DMA_MODE_NORMAL | LL_DMA_MEMORY_INCREMENT |
                LL_DMA_MDATAALIGN_HALFWORD | LL_DMA_PDATAALIGN_WORD | LL_DMA_PRIORITY_HIGH);
        LL_DMA_SetPeriphAddress(DMA1, SPEAKER_DMA_CHANNEL, (uint32_t)&TIM16->CCR1);
        LL_DMA_SetMemoryAddress(DMA1, SPEAKER_DMA_CHANNEL, (uint32_t)duty);
        LL_DMA_SetDataLength(DMA1, SPEAKER_DMA_CHANNEL, count + 1);
        LL_DMA_EnableChannel(DMA1, SPEAKER_DMA_CHANNEL);
        TIM16->DIER = TIM_DIER_UDE;
        TIM16->CR1 |= TIM_CR1_CEN;

        /* Include the final centre sample and time for the preload latch. */
        furi_delay_ms((count + 2) * 1000 / SPEAKER_PCM_RATE + 2);
        TIM16->DIER = 0;
        LL_DMA_DisableChannel(DMA1, SPEAKER_DMA_CHANNEL);
        LL_DMA_ClearFlag_GI3(DMA1);
        LL_DMA_SetPeriphRequest(DMA1, SPEAKER_DMA_CHANNEL, LL_DMAMUX_REQ_MEM2MEM);
    }
    furi_hal_speaker_release();
    free(duty);
}
