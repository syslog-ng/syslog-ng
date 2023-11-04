#ifndef FILTERX_OBJECT_H_INCLUDED
#define FILTERX_OBJECT_H_INCLUDED

#include "logmsg/logmsg.h"
#include <json-c/json.h>

typedef struct _FilterXType FilterXType;
typedef struct _FilterXObject FilterXObject;

struct _FilterXType
{
  FilterXType *super_type;
  const gchar *name;
  gboolean mutable;
  union
  {
    struct
    {
      FilterXObject *(*unmarshal)(FilterXObject *self);
      gboolean (*marshal)(FilterXObject *self, GString *repr, LogMessageValueType *t);
      FilterXObject *(*clone)(FilterXObject *self);
      gboolean (*map_to_json)(FilterXObject *self, struct json_object **object);
      gboolean (*truthy)(FilterXObject *self);
      FilterXObject *(*getattr)(FilterXObject *self, const gchar *attr_name);
      gboolean (*setattr)(FilterXObject *self, const gchar *attr_name, FilterXObject *new_value);
      void (*free_fn)(FilterXObject *self);
    };
    gpointer __methods[8];
  };
};

void filterx_type_init(FilterXType *type);

#define FILTERX_TYPE_NAME(_name) filterx_type_ ## _name
#define FILTERX_DECLARE_TYPE(_name) \
    extern FilterXType FILTERX_TYPE_NAME(_name)
#define FILTERX_DEFINE_TYPE(_name, _super_type, ...) \
    FilterXType FILTERX_TYPE_NAME(_name) = 	\
    {						\
      .super_type = &_super_type,		\
      .name = # _name,				\
      __VA_ARGS__				\
    }


FILTERX_DECLARE_TYPE(object);

struct _FilterXObject
{
  /* NOTE: this packs into 16 bytes in total (64 bit), let's try to keep
   * this small, potentially using bitfields.  A simple boolean is 32 bytes
   * in total at the moment (factoring in GenericNumber which is used to
   * represent it).  Maybe we could get rid off the GenericNumber wrapper
   * which would potentially decrease the struct to 16-24 bytes. */

  gint ref_cnt;

  /* NOTE:
   *
   *     modified_in_place -- set to TRUE in case the value in this
   *                          FilterXObject was changed
   *
   *     assigned          -- should be stored in the FilterXScope but we
   *                          can reuse a bit here.  Signifies if the value was assigned to a
   *                          name-value pair.
   *
   *     shadow            -- this object is a shadow of a LogMessage
   *                          name-value pair.  Whenever assigned to another name-value pair,
   *                          this needs to be copied.
   *
   */
  gint modified_in_place:1, shadow:1, assigned:1;
  FilterXType *type;
};

FilterXObject *filterx_object_new(FilterXType *type);
FilterXObject *filterx_object_ref(FilterXObject *self);
void filterx_object_unref(FilterXObject *self);
void filterx_object_init_instance(FilterXObject *self, FilterXType *type);
void filterx_object_free_method(FilterXObject *self);

static inline gboolean
filterx_object_is_type(FilterXObject *object, FilterXType *type)
{
  return object->type == type;
}

static inline FilterXObject *
filterx_object_unmarshal(FilterXObject *self)
{
  if (self->type->unmarshal)
    return self->type->unmarshal(self);
  return filterx_object_ref(self);
}

static inline gboolean
filterx_object_marshal(FilterXObject *self, GString *repr, LogMessageValueType *t)
{
  if (self->type->marshal)
    return self->type->marshal(self, repr, t);
  return FALSE;
}

static inline FilterXObject *
filterx_object_clone(FilterXObject *self)
{
  if (self->type->mutable)
    {
      /* mutable object that shadows a name-value pair must have clone */
      return self->type->clone(self);
    }
  /* unmutable objects don't need to be cloned */
  return filterx_object_ref(self);
}

static inline gboolean
filterx_object_map_to_json(FilterXObject *self, struct json_object **object)
{
  if (self->type->map_to_json)
    return self->type->map_to_json(self, object);
  return FALSE;
}

static inline gboolean
filterx_object_truthy(FilterXObject *self)
{
  return self->type->truthy(self);
}

static inline gboolean
filterx_object_falsy(FilterXObject *self)
{
  return !filterx_object_truthy(self);
}

static inline FilterXObject *
filterx_object_getattr(FilterXObject *self, const gchar *attr_name)
{
  if (self->type->getattr)
    return self->type->getattr(self, attr_name);
  return NULL;
}

static inline gboolean
filterx_object_setattr(FilterXObject *self, const gchar *attr_name, FilterXObject *new_value)
{
  if (self->type->setattr)
    return self->type->setattr(self, attr_name, new_value);
  return FALSE;
}

#endif
