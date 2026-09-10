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
#ifndef UNITEMP_MS5611
#define UNITEMP_MS5611

#include "../unitemp.h"
#include "../Sensors.h"

extern const SensorType MS5611;

/**
 * @brief Allocates memory and sets initial values for MS5611 sensor
 *
 * @param sensor Pointer to the sensor being created
 * @param args Arguments string (unused)
 * @return true on success
 */
bool unitemp_MS5611_alloc(Sensor* sensor, char* args);

/**
 * @brief Initializes the MS5611 sensor
 *
 * @param sensor Pointer to the sensor
 * @return true if initialization is successful
 */
bool unitemp_MS5611_init(Sensor* sensor);

/**
 * @brief Deinitializes the sensor
 *
 * @param sensor Pointer to the sensor
 * @return true
 */
bool unitemp_MS5611_deinit(Sensor* sensor);

/**
 * @brief Updates values from the sensor
 *
 * @param sensor Pointer to the sensor
 * @return Update status
 */
UnitempStatus unitemp_MS5611_update(Sensor* sensor);

/**
 * @brief Frees sensor memory
 *
 * @param sensor Pointer to the sensor
 * @return true
 */
bool unitemp_MS5611_free(Sensor* sensor);

#endif
