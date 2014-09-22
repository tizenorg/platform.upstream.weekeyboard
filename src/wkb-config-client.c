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
#include "wkb-log.h"

#include <Eldbus.h>
#include <Ecore.h>


static Eldbus_Connection *conn = NULL;
static Ecore_Timer *timeout = NULL;



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


static Eina_Bool
finish(void *data EINA_UNUSED)
{
   DBG("Timeout\nSome error happened or server is taking too much time to respond.");

   ecore_main_loop_quit();
   timeout = NULL;

   return ECORE_CALLBACK_CANCEL;
}

int
main(int argc, char* argv[])
{
   wkb_log_init("config-client");

   if (argc < 2)
     {
        ERR("Usage: %s <themename>\n\n", argv[0]);
        return ;
     }
   const char* theme = argv[1];

   DBG("sending %s", theme);

   Eldbus_Object *obj;
   Eldbus_Proxy *proxy;

   if (eina_init() <= 0)
     {
        EINA_LOG_ERR("Unable to init eina");
        goto exit_end;
     }   

   int log_domain = eina_log_domain_register("client", EINA_COLOR_CYAN);
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
   FILE* fp = popen("ibus address", "r");
   if (! fp)
     {
        ERR("Unable to find ibus address");
        goto exit_all;
     }

   char address[2048];
   if (! fgets(address, 2048 - 1, fp))
     {
        ERR("Unable to find ibus address");
        goto exit_all;
     }
   // and strip out the newline at the end
   address[strlen(address) - 1] = '\0';

   conn = eldbus_address_connection_get(address);

   if (! conn)
     {
        ERR("Cannot establish eldbus connection");
        goto exit_all;
     }

   obj = eldbus_object_get(conn,
                           IBUS_SERVICE_CONFIG, 
                           IBUS_PATH_CONFIG);
   if (! conn)
     {
        ERR("Cannot create eldbus object");
        goto exit_all;
     }

   proxy = eldbus_proxy_get(obj, IBUS_INTERFACE_CONFIG);
   if (! proxy)
     {
        ERR("Cannot create eldbus proxy");
        goto exit_all;
     }

   void* rval = eldbus_proxy_call(proxy, "SetTheme", _on_send_value, NULL, -1, "s", theme);
   if (! rval)
     {
        ERR("Cannot make eldbus proxy call");
        goto exit_all;
     }

   /*
      timeout = ecore_timer_add(30, finish, NULL);

      ecore_main_loop_begin();

      if (timeout) ecore_timer_del(timeout);
    */

   if (conn) eldbus_connection_unref(conn);

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
