#include <stdio.h>
#include <math.h>
#include "esp_log.h"
#include "volume.h"

#define MIN_VOLUME 0.0f  // Минимальная громкость 0.0f
#define MAX_VOLUME 1.0f  // Максимальная громкость 1.0f
#define STEPS 100        // Количество шагов громкости (в текущей реализации функции не используется)

#define VOLUME_CURVE_EXPONENT 1.5f // Коэффициент для степенной кривой громкости

/*
 * Таблица соответствия входных процентов громкости (0-100)
 * и выходных значений (0.000 - 1.000) по степенной кривой.
 * При 1% громкости возвращает 0.001f, при 100% - 1.000f.
 * Выход округляется до тысячных.
 * output = input ^ VOLUME_CURVE_EXPONENT
 * Формула: output = round( (input_percent / 100.0) ^ 1.5 * 1000) / 1000
 *
 * Input (%) | Output
 * --------- | ------
 * 0         | 0.000
 * 1         | 0.001
 * 10        | 0.032
 * 20        | 0.089
 * 30        | 0.164
 * 40        | 0.253
 * 50        | 0.354
 * 60        | 0.465
 * 70        | 0.586
 * 80        | 0.716
 * 90        | 0.852
 * 100       | 1.000
 */

float powerCurveVolumeControl(uint8_t inputVolumePercent) {
    if (inputVolumePercent == 0) {
        return 0.0f;
    }

    float normalizedInput = inputVolumePercent / 100.0f;

    float outputVolume = powf(normalizedInput, VOLUME_CURVE_EXPONENT);

    outputVolume = roundf(outputVolume * 1000.0f) / 1000.0f;

    outputVolume = fmaxf(0.0f, fminf(outputVolume, 1.0f));

    if (inputVolumePercent == 100) {
        return 1.0f;
    }

    return outputVolume;
}
