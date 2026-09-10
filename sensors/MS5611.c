/*
    Unitemp - Universal temperature reader
    Copyright (C) 2022-2023  Victor Nikitchuk (https://github.com/quen0n)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/
#include "MS5611.h"
#include "../interfaces/I2CSensor.h"

// MS5611 Commands
#define MS5611_CMD_RESET 0x1E
#define MS5611_CMD_ADC_READ 0x00
#define MS5611_CMD_ADC_CONV 0x40
#define MS5611_CMD_ADC_D1 0x00
#define MS5611_CMD_ADC_D2 0x10
#define MS5611_CMD_ADC_4096 0x08
#define MS5611_CMD_PROM_RD 0xA0

typedef struct {
    uint16_t C[7]; // C1 to C6 (index 0 unused or CRC)
} MS5611_cal;

typedef struct {
    MS5611_cal cal;
} MS5611_instance;

const SensorType MS5611 = {
    .typename = "MS5611",
    .interface = &I2C,
    .datatype = UT_TEMPERATURE | UT_PRESSURE,
    .pollingInterval = 1000,
    .allocator = unitemp_MS5611_alloc,
    .mem_releaser = unitemp_MS5611_free,
    .initializer = unitemp_MS5611_init,
    .deinitializer = unitemp_MS5611_deinit,
    .updater = unitemp_MS5611_update};

bool unitemp_MS5611_alloc(Sensor* sensor, char* args) {
    UNUSED(args);
    I2CSensor* i2c_sensor = (I2CSensor*)sensor->instance;

    // Supported I2C addresses: 0x76 and 0x77 (shifted by 1 for HAL)
    i2c_sensor->minI2CAdr = 0x76 << 1;
    i2c_sensor->maxI2CAdr = 0x77 << 1;

    MS5611_instance* instance = malloc(sizeof(MS5611_instance));
    if(instance == NULL) {
        FURI_LOG_E(APP_NAME, "Failed to allocate MS5611 instance");
        return false;
    }
    i2c_sensor->sensorInstance = instance;
    return true;
}

bool unitemp_MS5611_free(Sensor* sensor) {
    I2CSensor* i2c_sensor = (I2CSensor*)sensor->instance;
    if(i2c_sensor->sensorInstance != NULL) {
        free(i2c_sensor->sensorInstance);
    }
    return true;
}

bool unitemp_MS5611_init(Sensor* sensor) {
    I2CSensor* i2c_sensor = (I2CSensor*)sensor->instance;
    MS5611_instance* instance = (MS5611_instance*)i2c_sensor->sensorInstance;

    // Reset
    uint8_t cmd = MS5611_CMD_RESET;
    if(!unitemp_i2c_writeArray(i2c_sensor, 1, &cmd)) return false;
    furi_delay_ms(5); // Wait for reset

    // Read PROM (C1 to C6)
    // PROM addresses 0xA2 to 0xAC for C1 to C6.
    // The coefficients are 16-bit unsigned integers.
    // Loop C1..C6. Address = 0xA0 + i*2.
    // Actually we need C1 to C6.
    // 0xA0 is Reserved/CRC.
    // 0xA2 is C1.
    // ...
    // 0xAE is Reserved/CRC? No.
    // Datasheet:
    // 0xA0: Reserved / CRC
    // 0xA2: C1
    // 0xA4: C2
    // 0xA6: C3
    // 0xA8: C4
    // 0xAA: C5
    // 0xAC: C6
    // 0xAE: Reserved / CRC

    // We only need C1-C6.

    uint8_t buff[2];
    for(int i = 1; i <= 6; i++) {
        cmd = MS5611_CMD_PROM_RD + (i * 2);
        // Note: unitemp_i2c_readRegArray writes the register address (cmd) then reads
        if(!unitemp_i2c_readRegArray(i2c_sensor, cmd, 2, buff)) {
             FURI_LOG_E(APP_NAME, "MS5611: Failed to read coefficient %d", i);
             return false;
        }
        instance->cal.C[i] = (buff[0] << 8) | buff[1];
    }

    // Check if coefficients are valid (not all zero)
    if(instance->cal.C[1] == 0 && instance->cal.C[2] == 0) {
         FURI_LOG_E(APP_NAME, "MS5611: Coefficients are zero");
         return false;
    }

    UNITEMP_DEBUG("MS5611 Coeffs: %d %d %d %d %d %d",
        instance->cal.C[1], instance->cal.C[2], instance->cal.C[3],
        instance->cal.C[4], instance->cal.C[5], instance->cal.C[6]);

    return true;
}

bool unitemp_MS5611_deinit(Sensor* sensor) {
    UNUSED(sensor);
    return true;
}

UnitempStatus unitemp_MS5611_update(Sensor* sensor) {
    I2CSensor* i2c_sensor = (I2CSensor*)sensor->instance;
    MS5611_instance* instance = (MS5611_instance*)i2c_sensor->sensorInstance;

    uint8_t cmd;
    uint8_t buff[3];

    // 1. Read Temperature (D2)
    cmd = MS5611_CMD_ADC_CONV | MS5611_CMD_ADC_D2 | MS5611_CMD_ADC_4096;
    if(!unitemp_i2c_writeArray(i2c_sensor, 1, &cmd)) return UT_SENSORSTATUS_TIMEOUT;
    furi_delay_ms(10); // Max conversion time for OSR=4096 is ~9ms

    if(!unitemp_i2c_readRegArray(i2c_sensor, MS5611_CMD_ADC_READ, 3, buff)) return UT_SENSORSTATUS_TIMEOUT;
    uint32_t D2 = ((uint32_t)buff[0] << 16) | ((uint32_t)buff[1] << 8) | buff[2];

    // 2. Read Pressure (D1)
    cmd = MS5611_CMD_ADC_CONV | MS5611_CMD_ADC_D1 | MS5611_CMD_ADC_4096;
    if(!unitemp_i2c_writeArray(i2c_sensor, 1, &cmd)) return UT_SENSORSTATUS_TIMEOUT;
    furi_delay_ms(10);

    if(!unitemp_i2c_readRegArray(i2c_sensor, MS5611_CMD_ADC_READ, 3, buff)) return UT_SENSORSTATUS_TIMEOUT;
    uint32_t D1 = ((uint32_t)buff[0] << 16) | ((uint32_t)buff[1] << 8) | buff[2];

    // 3. Calculate
    // Use int64_t to prevent overflow during intermediate calculations

    int64_t dT = (int64_t)D2 - ((int64_t)instance->cal.C[5] << 8);
    int64_t TEMP = 2000 + ((dT * (int64_t)instance->cal.C[6]) >> 23);

    int64_t OFF = ((int64_t)instance->cal.C[2] << 16) + (((int64_t)instance->cal.C[4] * dT) >> 7);
    int64_t SENS = ((int64_t)instance->cal.C[1] << 15) + (((int64_t)instance->cal.C[3] * dT) >> 8);

    // Second order temperature compensation
    if(TEMP < 2000) {
        int64_t T2 = (dT * dT) >> 31;
        int64_t OFF2 = (5 * (TEMP - 2000) * (TEMP - 2000)) >> 1;
        int64_t SENS2 = (5 * (TEMP - 2000) * (TEMP - 2000)) >> 2;

        if(TEMP < -1500) {
             int64_t temp_diff = TEMP + 1500;
             OFF2 = OFF2 + 7 * temp_diff * temp_diff;
             SENS2 = SENS2 + 11 * temp_diff * temp_diff;
        }

        TEMP -= T2;
        OFF -= OFF2;
        SENS -= SENS2;
    }

    int64_t P = ((D1 * SENS) >> 21) - OFF;
    P = P >> 15;

    sensor->temp = (float)TEMP / 100.0f;
    sensor->pressure = (float)P; // Pressure in Pascals. Unitemp seems to use Pascals usually?
    // BMP180 implementation: P is in Pascals.

    return UT_SENSORSTATUS_OK;
}
