/*
 * Copyright © 2014 Jaguar Landrover
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

#include "wkb-ibus-defs.h"
//#include "wkb-log.h"

#define DBG printf("\n"); printf
#define ERR printf("\n"); printf

#include <Eldbus.h>
#include <Ecore.h>
#include <stdio.h>

static Eldbus_Connection *conn = NULL;

static void
on_send(void *data EINA_UNUSED, const Eldbus_Message *msg EINA_UNUSED, Eldbus_Pending *pending EINA_UNUSED)
{
   printf("on_send()\n\n");
}

/*
static void
_on_send_value(void *data EINA_UNUSED, const Eldbus_Message *msg, Eldbus_Pending *pending EINA_UNUSED)
{
   const char *errname, *errmsg;
   char *s;

   DBG("_on_send_value");

   if (eldbus_message_error_get(msg, &errname, &errmsg))
     {
        ERR("%s %s", errname, errmsg);
        return ;
     }

   if (!eldbus_message_arguments_get(msg, "s", &s))
     {
        ERR("Could not get entry contents");
        return ;
     }

   DBG("Recieved back from SetValue: %s", s);
}
*/

static Eina_Bool
finish(void *data EINA_UNUSED)
{
   DBG("Timeout\nSome error happened or server is taking too much time to respond.");

   ecore_main_loop_quit();

   return ECORE_CALLBACK_CANCEL;
}

int
main(int argc, char *argv[])
{
   const char *theme = argv[1];
   int log_domain;
   FILE *fp;
   char address[2048];

   Eldbus_Object *obj;
   Eldbus_Proxy *proxy;
   Eldbus_Message *msg;
   Eldbus_Pending *pending;

   wkb_log_init("config-client");

   if (argc < 2)
     {
        ERR("Usage: %s <theme name>\n\n", argv[0]);
        return ;
     }
   DBG("sending %s", theme);

   if (eina_init() <= 0)
     {
        EINA_LOG_ERR("Unable to init eina");
        goto exit_end;
     }

   log_domain = eina_log_domain_register("client", EINA_COLOR_CYAN);
   if (log_domain < 0)
     {
        EINA_LOG_ERR("Unable to create 'client' log domain");
        goto exit_eina;
     }

   if (ecore_init() <= 0)
     {
        EINA_LOG_ERR("Unable to initialize ecore");
        goto exit_eina;
     }

   if (eldbus_init() <= 0)
     {
        EINA_LOG_ERR("Unable to initialize eldbus");
        goto exit_ecore;
     }

   // get the connection address. Yes, it only comes from a command line application
   fp = popen("ibus address", "r");
   if (!fp)
     {
        ERR("Unable to find ibus address");
        goto exit_all;
     }

   if (!fgets(address, 2048 - 1, fp))
     {
        ERR("Unable to find ibus address");
        goto exit_all;
     }
   // and strip out the newline at the end
   address[strlen(address) - 1] = '\0';

   printf("\naddress = <%s>\n", address);

   conn = eldbus_address_connection_get(address);

   printf("\nconn = 0x%p\n", conn);
   if (!conn)
     {
        ERR("Cannot establish eldbus connection");
        goto exit_all;
     }

   obj = eldbus_object_get(conn,
                           IBUS_SERVICE_CONFIG, //IBUS_SERVICE_PANEL, //
                           IBUS_PATH_CONFIG); //IBUS_PATH_PANEL);  //
   if (!obj)
     {
        ERR("Cannot create eldbus object");
        goto exit_all;
     }

   proxy = eldbus_proxy_get(obj, IBUS_INTERFACE_CONFIG);//IBUS_INTERFACE_PANEL);
   if (!proxy)
     {
        ERR("Cannot create eldbus proxy");
        goto exit_all;
     }

   msg = eldbus_proxy_method_call_new(proxy, "SetValue");
   Eldbus_Message_Iter* iter = eldbus_message_iter_get(msg);

   printf("iter = 0x%p\n", iter);  fflush(stdout);

//   Eldbus_Message_Iter* parms;

   Eldbus_Message_Iter* variant;
   eldbus_message_iter_arguments_append(iter, "ss", "panel", "theme");
   variant = eldbus_message_iter_container_new(iter, 'v', "s");
   printf("variant = 0x%p\n", variant);  fflush(stdout);

   if (! eldbus_message_iter_basic_append(variant, 's', theme))
   {
       printf("append of panel string failed\n"); fflush(stdout);
   }

   eldbus_message_iter_container_close(iter, variant);

   pending = eldbus_proxy_send(proxy, msg, on_send, NULL, -1);
   if (!pending)
     {
        ERR("Cannot make eldbus proxy call");
        goto exit_all;
     }

   eldbus_connection_unref(conn);

exit_all:
exit_eldbus:
   eldbus_shutdown();

exit_ecore:
   ecore_shutdown();

exit_eina:
   eina_log_domain_unregister(log_domain);
   eina_shutdown();

exit_end:

   return 0;
}
