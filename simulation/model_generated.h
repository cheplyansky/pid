#pragma once
#ifndef MODEL_GENERATED_H
#define MODEL_GENERATED_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char* name;
    double* ptr;
    int dir; /* 0=out, 1=in */
} model_ExtPort;

typedef struct model_Model model_Model;

struct model_Model {
    double setpoint;
    double feedback;
    double Add1;
    double Add2;
    double Add3;
    double I_gain;
    double P_gain;
    double Ts;
    double Unit_Delay1;
};

void model_init(model_Model* model);
void model_step(model_Model* model);
const model_ExtPort* model_get_ext_ports(model_Model* model);

#ifdef __cplusplus
}
#endif

#endif
