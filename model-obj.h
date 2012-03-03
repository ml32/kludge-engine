#ifndef KL_MDLOBJ_H
#define KL_MDLOBJ_H

#include "model.h"

#include <stdint.h>
#include <stdbool.h>

bool kl_model_isobj(uint8_t *data, int size);
kl_model_t* kl_model_loadobj(uint8_t *data, int size);

#endif /* KL_MDLOBJ_H */

/* vim: set ts=2 sw=2 et */
