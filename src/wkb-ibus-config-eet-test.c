/*
 * Copyright © 2013 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "wkb-ibus-config-eet.h"

#include <Eina.h>
#include <Eet.h>

int
main (int argc, char *argv[])
{
   struct wkb_ibus_config_eet *cfg;

   if (!eina_init())
     {
        fprintf(stderr,"Error initializing eina");
        return 1;
     }

   if (!eet_init())
     {
        fprintf(stderr,"Error initializing eet");
        return 1;
     }

   cfg = wkb_ibus_config_eet_new("ibus-cfg.eet");
   wkb_ibus_config_eet_free(cfg);

   eet_shutdown();
   eina_shutdown();

   return 0;
}
