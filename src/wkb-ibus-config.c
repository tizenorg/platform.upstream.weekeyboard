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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <Eina.h>
#include <Eldbus.h>

#include "wkb-ibus.h"
#include "wkb-ibus-defs.h"
#include "wkb-ibus-config-eet.h"
#include "wkb-log.h"

static struct wkb_ibus_config_eet *_conf_eet = NULL;

#define _config_check_message_errors(_msg)				\
   do									\
{									\
   const char *error, *error_msg;					\
   if (eldbus_message_error_get(_msg, &error, &error_msg))		\
     {								\
        ERR("DBus message error: %s: %s", error, error_msg);		\
        return NULL;							\
     }								\
   DBG("Message '%s' with signature '%s'", eldbus_message_member_get(_msg), eldbus_message_signature_get(_msg)); \
} while (0)

static Eldbus_Message *
_config_set_value(const Eldbus_Service_Interface *iface, const Eldbus_Message *msg)
{
   const char *section, *name;
   Eldbus_Message_Iter *value;

   DBG("in _config_set_value");

   _config_check_message_errors(msg);

   if (!eldbus_message_arguments_get(msg, "ssv", &section, &name, &value))
     {
        ERR("Error reading message arguments");
        return NULL;
     }

   DBG("section: '%s', name: '%s', value: '%p'", section, name, value);

   wkb_ibus_config_eet_set_value(_conf_eet, section, name, value);

   return NULL;
}

static Eldbus_Message *
_config_get_value(const Eldbus_Service_Interface *iface, const Eldbus_Message *msg)
{
   const char *section, *name;
   Eldbus_Message *reply = NULL;
   Eldbus_Message_Iter *iter;

   _config_check_message_errors(msg);

   if (!eldbus_message_arguments_get(msg, "ss", &section, &name))
     {
        ERR("Error reading message arguments");
        return NULL;
     }

   DBG("section: '%s', name: '%s'", section, name);

   reply = eldbus_message_method_return_new(msg);
   iter = eldbus_message_iter_get(reply);
   wkb_ibus_config_eet_get_value(_conf_eet, section, name, iter);

   return reply;
}

static Eldbus_Message *
_config_get_values(const Eldbus_Service_Interface *iface, const Eldbus_Message *msg)
{
   const char *section;
   Eldbus_Message *reply = NULL;
   Eldbus_Message_Iter *iter;

   _config_check_message_errors(msg);

   if (!eldbus_message_arguments_get(msg, "s", &section))
     {
        ERR("Error reading message arguments");
        return NULL;
     }

   DBG("section: '%s'", section);

   reply = eldbus_message_method_return_new(msg);
   iter = eldbus_message_iter_get(reply);
   wkb_ibus_config_eet_get_values(_conf_eet, section, iter);

   return reply;
}

static Eldbus_Message *
_config_unset_value(const Eldbus_Service_Interface *iface, const Eldbus_Message *msg)
{
   const char *section, *name;

   _config_check_message_errors(msg);

   if (!eldbus_message_arguments_get(msg, "ss", &section, &name))
     {
        ERR("Error reading message arguments");
        return NULL;
     }

   DBG("section: '%s', name: '%s'", section, name);

   wkb_ibus_config_eet_set_value(_conf_eet, section, name, NULL);

   return NULL;
}

/* Themes. To support changing themes dynamically, a Set theme was added to
 * the config interface.
 *
 * Unlike the Set/Get values, this is a simple string, and is stored as a
 * global. If any more sphisticated options are added, this should be
 * expanded. In particular, it could be made persistent.
 */

// At this level the theme is just a string. 
static char _theme[PATH_MAX] = "";

const char* wkb_ibus_get_theme()
{
   return _theme;
}

/* wkb_ibus_theme_changes - support a single callback method for when the
 *    theme is changed
 *
 * TODO - this is too simple, it should support multiple callcacks or a
 * signal of some sort */
static void* _theme_cb_data = NULL;
static wkb_theme_changed_cb _notify_theme_cb = NULL;

void
wkb_ibus_theme_changes(wkb_theme_changed_cb cb, void * data)
{
   _notify_theme_cb = cb;
   _theme_cb_data = data;
}

/* Implementation of the SetTheme configuration interface. Gets the string defining
 * the new theme, and notifies any registered callback */
static Eldbus_Message *
_config_set_theme(const Eldbus_Service_Interface *iface, const Eldbus_Message *msg)
{
   const char *theme;

   DBG("in _config_set_theme");

   _config_check_message_errors(msg);

   if (!eldbus_message_arguments_get(msg, "s", &theme))
     {
        ERR("Error reading theme message arguments");
        return NULL;
     }

   strncpy(_theme, theme, PATH_MAX - 1);
   DBG("theme: '%s'", _theme);

   if (_notify_theme_cb)
     {
        _notify_theme_cb(_theme, _theme_cb_data);
     }

   return NULL;
}

static const Eldbus_Method _wkb_ibus_config_methods[] =
{
   /* typedef struct _Eldbus_Method
    * {
    *    const char *member;
    *    const Eldbus_Arg_Info *in;
    *    const Eldbus_Arg_Info *out;
    *    Eldbus_Method_Cb cb;
    *    unsigned int flags;
    * } Eldbus_Method;
    */
     { .member = "SetValue",
        .in = ELDBUS_ARGS({"s", "section"}, {"s", "name"}, {"v", "value"}),
        .cb = _config_set_value, },

     { .member = "GetValue",
        .in = ELDBUS_ARGS({"s", "section"}, {"s", "name"}),
        .out = ELDBUS_ARGS({"v", "value"}),
        .cb = _config_get_value, },

     { .member = "GetValues",
        .in = ELDBUS_ARGS({"s", "section"}),
        .out = ELDBUS_ARGS({"a{sv}", "values"}),
        .cb = _config_get_values, },

     { .member = "UnsetValue",
        .in = ELDBUS_ARGS({"s", "section"}, {"s", "name"}),
        .cb = _config_unset_value, },

     /* SetTheme - interface to send a new "theme" as a string. This
      * should be the path to a .edj file for the UI layer to use.
      */
     { .member = "SetTheme",
        .in = ELDBUS_ARGS({"s", "path"}),
        .cb = _config_set_theme, },   

     { NULL },
};

static const Eldbus_Signal _wkb_ibus_config_signals[] =
{
   /* typedef struct _Eldbus_Signal
    * {
    *    const char *name;
    *    const Eldbus_Arg_Info *args;
    *    unsigned int flags;
    * } Eldbus_Signal;
    */
     { .name = "ValueChanged",
        .args = ELDBUS_ARGS({"s", "section"}, {"s", "name"}, {"v", "value"}),
        .flags = 0, },

     { NULL },
};

static const Eldbus_Service_Interface_Desc _wkb_ibus_config_interface =
{
   .interface = IBUS_INTERFACE_CONFIG,
   .methods = _wkb_ibus_config_methods,
   .signals = _wkb_ibus_config_signals,
};

Eldbus_Service_Interface *
wkb_ibus_config_register(Eldbus_Connection *conn, const char *path)
{
   Eldbus_Service_Interface *ret = NULL;

   if (_conf_eet)
     {
        WRN("wkb_config_eet already created\n");
        goto end;
     }

   DBG("eldbus_service_interface_register");
   if (!(ret = eldbus_service_interface_register(conn, IBUS_PATH_CONFIG, &_wkb_ibus_config_interface)))
     {
        ERR("Unable to register IBusConfig interface\n");
        goto end;
     }

   _conf_eet = wkb_ibus_config_eet_new(path, ret);

   if (!_conf_eet)
     {
        eldbus_service_interface_unregister(ret);
        ret = NULL;
     }

end:
   return ret;
}

void
wkb_ibus_config_unregister(void)
{
   if (!_conf_eet)
      return;

   wkb_ibus_config_eet_free(_conf_eet);
   _conf_eet = NULL;
}
