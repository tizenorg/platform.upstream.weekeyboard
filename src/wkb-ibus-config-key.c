/*
 * Copyright © 2013 Intel Corporation
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

#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>

#include "wkb-ibus-config-key.h"
#include "wkb-log.h"

typedef void (*key_free_cb) (void *);
typedef Eina_Bool (*key_set_cb) (struct wkb_config_key *, Eldbus_Message_Iter *);
typedef Eina_Bool (*key_get_cb) (struct wkb_config_key *, Eldbus_Message_Iter *);

struct wkb_config_key
{
   const char *id;
   const char *section;
   const char *signature;
   void *field; /* pointer to the actual struct field */

   key_free_cb free;
   key_set_cb set;
   key_get_cb get;
};

static struct wkb_config_key *
_key_new(const char *id, const char *section, const char *signature, void *field, key_free_cb free_cb, key_set_cb set_cb, key_get_cb get_cb)
{
   struct wkb_config_key *key = calloc(1, sizeof(*key));
   key->id = eina_stringshare_add(id);
   key->section = eina_stringshare_add(section);
   key->signature = eina_stringshare_add(signature);
   key->field = field;
   key->free = free_cb;
   key->set = set_cb;
   key->get = get_cb;
   return key;
}

#define _key_basic_set(_key, _type) \
   do { \
        _type __value = 0; \
        _type *__field = (_type *) _key->field; \
        if (!eldbus_message_iter_arguments_get(iter, _key->signature, &__value)) \
          { \
             ERR("Error decoding " #_type " value using '%s'", _key->signature); \
             return EINA_FALSE; \
          } \
        *__field = __value; \
        return EINA_TRUE; \
   } while (0)

#define _key_basic_get(_key, _type, _iter) \
   do { \
        _type *__field = (_type *) _key->field; \
       return eldbus_message_iter_basic_append(_iter, *_key->signature, *__field); \
   } while (0)

static Eina_Bool
_key_int_set(struct wkb_config_key *key, Eldbus_Message_Iter *iter)
{
   _key_basic_set(key, int);
}

static Eina_Bool
_key_int_get(struct wkb_config_key *key, Eldbus_Message_Iter *reply)
{
   _key_basic_get(key, int, reply);
}

static Eina_Bool
_key_bool_set(struct wkb_config_key *key, Eldbus_Message_Iter *iter)
{
   _key_basic_set(key, Eina_Bool);
}

static Eina_Bool
_key_bool_get(struct wkb_config_key *key, Eldbus_Message_Iter *reply)
{
   _key_basic_get(key, Eina_Bool, reply);
}

static void
_key_string_free(const char **str)
{
   if (!*str)
      return;

   eina_stringshare_del(*str);
   *str = NULL;
}

static Eina_Bool
_key_string_set(struct wkb_config_key *key, Eldbus_Message_Iter *iter)
{
   const char *str = NULL;
   const char **field;

   if (iter && !eldbus_message_iter_arguments_get(iter, "s", &str))
     {
        ERR("Error decoding string value using 's'");
        return EINA_FALSE;
     }

   if ((field = (const char **) key->field))
      _key_string_free(field);

   if (str)
      *field = eina_stringshare_add(str);

   INF("Setting key <%s/%s> to <%s>", key->section, key->id, *field);

   return EINA_TRUE;
}

static Eina_Bool
_key_string_get(struct wkb_config_key *key, Eldbus_Message_Iter *reply)
{
   _key_basic_get(key, const char *, reply);
}

static void
_key_string_list_free(Eina_List **list)
{
   const char *str;

   if (!*list)
      return;

   EINA_LIST_FREE(*list, str)
      eina_stringshare_del(str);

   eina_list_free(*list);
   *list = NULL;
}

static Eina_Bool
_key_string_list_set(struct wkb_config_key *key, Eldbus_Message_Iter *iter)
{
   const char *str;
   Eina_List *list = NULL;
   Eina_List **field;
   Eldbus_Message_Iter *array = NULL;

   if (!eldbus_message_iter_arguments_get(iter, "as", &array))
     {
        ERR("Expecting 'as' got '%s'", eldbus_message_iter_signature_get(iter));
        return EINA_FALSE;
     }

   while (eldbus_message_iter_get_and_next(array, 's', &str))
      list = eina_list_append(list, eina_stringshare_add(str));

   if ((field = (Eina_List **) key->field))
      _key_string_list_free(field);

   *field = list;

   return EINA_TRUE;
}

static Eina_Bool
_key_string_list_get(struct wkb_config_key *key, Eldbus_Message_Iter *reply)
{
   Eina_List *node, **list = (Eina_List **) key->field;
   const char *str;
   Eldbus_Message_Iter *array;

   array = eldbus_message_iter_container_new(reply, 'a', "s");

   EINA_LIST_FOREACH(*list, node, str)
      eldbus_message_iter_basic_append(array, 's', str);

   eldbus_message_iter_container_close(reply, array);

   return EINA_TRUE;
}

/*
 * PUBLIC FUNCTIONS
 */
struct wkb_config_key *
wkb_config_key_int(const char *id, const char *section, void *field)
{
   return _key_new(id, section, "i", field, NULL, _key_int_set, _key_int_get);
}

struct wkb_config_key *
wkb_config_key_bool(const char *id, const char *section, void *field)
{
   return _key_new(id, section, "b", field, NULL, _key_bool_set, _key_bool_get);
}

struct wkb_config_key *
wkb_config_key_string(const char *id, const char *section, void *field)
{
   return _key_new(id, section, "s", field, (key_free_cb) _key_string_free, _key_string_set, _key_string_get);
}

struct wkb_config_key *
wkb_config_key_string_list(const char *id, const char *section, void *field)
{
   return _key_new(id, section, "as", field, (key_free_cb) _key_string_list_free, _key_string_list_set, _key_string_list_get);
}

void
wkb_config_key_free(struct wkb_config_key *key)
{
   if (key->free && key->field)
      key->free(key->field);

   eina_stringshare_del(key->id);
   eina_stringshare_del(key->section);
   eina_stringshare_del(key->signature);
   free(key);
}

const char *
wkb_config_key_id(struct wkb_config_key *key)
{
   return key->id;
}

const char *
wkb_config_key_section(struct wkb_config_key *key)
{
   return key->section;
}

const char *
wkb_config_key_signature(struct wkb_config_key *key)
{
   return key->signature;
}

Eina_Bool
wkb_config_key_set(struct wkb_config_key * key, Eldbus_Message_Iter *iter)
{
   if (!key->field || !key->set)
      return EINA_FALSE;

   return key->set(key, iter);
}

Eina_Bool
wkb_config_key_get(struct wkb_config_key *key, Eldbus_Message_Iter *reply)
{
   Eina_Bool ret = EINA_FALSE;
   Eldbus_Message_Iter *value;

   if (!key->field || !key->get)
      return EINA_FALSE;

   value = eldbus_message_iter_container_new(reply, 'v', key->signature);

   if (!(ret = key->get(key, value)))
     {
        ERR("Unexpected error retrieving value for key: '%s'", key->id);
     }

   eldbus_message_iter_container_close(reply, value);

   return ret;
}

int
wkb_config_key_get_int(struct wkb_config_key* key)
{
   assert(!strcmp(key->signature, "i"));

   return *((int *) key->field);
}

Eina_Bool
wkb_config_key_get_bool(struct wkb_config_key* key)
{
   assert(!strcmp(key->signature, "b"));

   return *((Eina_Bool *) key->field);
}

const char *
wkb_config_key_get_string(struct wkb_config_key* key)
{
   DBG("Found key: id = <%s> signature = <%s> field = 0x%p", key->id, key->signature, key->field);
   DBG("Found key: id = <%s> signature = <%s> field as string = <%s>", key->id, key->signature, *(const char **)key->field);

   assert(!strcmp(key->signature, "s"));

   return *((const char **) key->field);
}

char **
wkb_config_key_get_string_list(struct wkb_config_key *key)
{
   Eina_List *node, **list = (Eina_List **) key->field;
   char *str, **ret;
   int i = 0;

   assert(!strcmp(key->signature, "as"));

   ret = calloc(eina_list_count(*list) + 1, sizeof(char *));
   EINA_LIST_FOREACH(*list, node, str)
      ret[i++] = str;

   return ret;
}

