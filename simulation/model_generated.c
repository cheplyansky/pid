#include "model_generated.h"

void model_init(model_Model* model)
{
    if (!model) return;
    model->setpoint = 0.0;
    model->feedback = 0.0;
    model->Add1 = 0.0;
    model->Add2 = 0.0;
    model->Add3 = 0.0;
    model->I_gain = 0.0;
    model->P_gain = 0.0;
    model->Ts = 0.0;
    model->Unit_Delay1 = 0.0;
}

void model_step(model_Model* model)
{
    if (!model) return;

    model->Add1 = (*model).setpoint - (*model).feedback;
    model->I_gain = (*model).Add1 * (2);
    model->P_gain = (*model).Add1 * (3);
    model->Ts = (*model).I_gain * (0.01);
    model->Add2 = (*model).Ts + (*model).Unit_Delay1;
    model->Add3 = (*model).P_gain + (*model).Add2;

    model->Unit_Delay1 = (*model).Add2;
}

const model_ExtPort* model_get_ext_ports(model_Model* model)
{
    static model_ExtPort ports[4];
    int i = 0;

    ports[i].name = "setpoint";
    ports[i].ptr = model ? &model->setpoint : 0;
    ports[i].dir = 1;
    ++i;

    ports[i].name = "feedback";
    ports[i].ptr = model ? &model->feedback : 0;
    ports[i].dir = 1;
    ++i;

    ports[i].name = "command";
    ports[i].ptr = model ? &model->Add3 : 0;
    ports[i].dir = 0;
    ++i;

    ports[i].name = 0;
    ports[i].ptr = 0;
    ports[i].dir = 0;
    return ports;
}
