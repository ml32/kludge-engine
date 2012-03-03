#ifndef KL_MDLIQM2_H
#define KL_MDLIQM2_H

#include "model.h"

#include <stdint.h>
#include <stdbool.h>

bool kl_model_isiqm2(uint8_t *data, int size);
kl_model_t* kl_model_loadiqm2(uint8_t *data, int size);

#endif /* KL_MDLIQM2_H */

/* vim: set ts=2 sw=2 et */
