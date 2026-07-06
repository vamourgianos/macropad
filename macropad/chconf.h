/*
    ChibiOS - Copyright (C) 2006..2018 Giovanni Di Sirio

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

/**
 * @file    rt/templates/chconf.h
 * @brief   Configuration file template.
 * @details A copy of this file must be placed in each project directory, it
 *          contains the application specific kernel settings.
 *
 * @addtogroup config
 * @details Kernel related settings and hooks.
 * @{
 */

/*
 * This file was auto-migrated from an older version by:
 *    `qmk chibios-confmigrate -i keyboards/m_star_classic/chconf.h -r platforms/chibios/common/configs/chconf.h`
 */

#pragma once

#define CH_CFG_ST_TIMEDELTA 0

#define CH_CFG_USE_REGISTRY TRUE

#define CH_CFG_USE_WAITEXIT TRUE

#define CH_CFG_USE_CONDVARS TRUE

#define CH_CFG_USE_CONDVARS_TIMEOUT FALSE

#define CH_CFG_USE_MESSAGES TRUE

#define CH_CFG_USE_MAILBOXES TRUE

#define CH_CFG_USE_HEAP TRUE

#define CH_CFG_USE_OBJ_CACHES false

#define CH_CFG_USE_DELEGATES false

#define CH_CFG_USE_JOBS false

#include_next <chconf.h>
