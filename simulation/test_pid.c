// test_closed_loop_with_output.c
#include "model_generated.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <sys/stat.h>  // для mkdir

// Функция для создания директории
void create_directory(const char* path) {
    #ifdef _WIN32
        mkdir(path);
    #else
        mkdir(path, 0755);
    #endif
}

double system_response(double command, double current_output, double dt, double Tc) {
    return current_output + (command - current_output) * dt / Tc;
}

int main() {
    model_Model model;
    const model_ExtPort* ports;
    
    model_init(&model);
    
    double* setpoint_ptr = NULL;
    double* feedback_ptr = NULL;
    double* command_ptr = NULL;
    
    ports = model_get_ext_ports(&model);
    for (int i = 0; ports[i].name; i++) {
        if (strcmp(ports[i].name, "setpoint") == 0)
            setpoint_ptr = ports[i].ptr;
        else if (strcmp(ports[i].name, "feedback") == 0)
            feedback_ptr = ports[i].ptr;
        else if (strcmp(ports[i].name, "command") == 0)
            command_ptr = ports[i].ptr;
    }
    
    // === ПАРАМЕТРЫ ===
    double target = 1.0;           // Цель
    double time_constant = 1.0;    // Постоянная времени
    double dt = 0.01;              // Шаг
    double tolerance = 0.001;      // Точность
    
    int max_steps = 10000;         // Максимальное количество шагов
    int print_interval = 10;       // Печатать каждый N-ый шаг (после первых 10)
    
    *setpoint_ptr = target;
    *feedback_ptr = 0.0;
    
    // Создаем директорию results, если её нет
    create_directory("results");
    
    // Открываем файл для сохранения результатов
    FILE* data_file = fopen("./results/simulation_data.csv", "w");
    if (!data_file) {
        printf("Warning: Cannot create data file\n");
    } else {
        fprintf(data_file, "Step,Time,Command,Feedback,Error,P_out,I_out,Integrator\n");
    }
    
    printf("\n=== PID Control Simulation ===\n");
    printf("Target: %.1f\n", target);
    printf("Time constant: %.1f\n", time_constant);
    printf("Tolerance: %.0e\n\n", tolerance);
    
    printf("+-------+----------+----------+----------+--------+--------+------------+\n");
    printf("| Step  | Command  | Feedback | Error    | P_out  | I_out  | Integrator |\n");
    printf("+-------+----------+----------+----------+--------+--------+------------+\n");
    
    int step = 0;
    double error = target - *feedback_ptr;
    double time = 0.0;
    
    // Step 0 (всегда выводим) - используем model.P_gain
    printf("| %5d | %8.4f | %8.4f | %8.4f | %6.2f | %6.2f | %10.6f |\n",
           step, *command_ptr, *feedback_ptr, error, 
           model.P_gain, model.I_gain, model.Unit_Delay1);
    
    if (data_file) {
        fprintf(data_file, "%d,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f\n",
                step, time, *command_ptr, *feedback_ptr, error,
                model.P_gain, model.I_gain, model.Unit_Delay1);
    }
    
    // Основной цикл
    while (fabs(error) > tolerance && step < max_steps) {
        step++;
        time += dt;
        
        model_step(&model);
        *feedback_ptr = system_response(*command_ptr, *feedback_ptr, dt, time_constant);
        error = target - *feedback_ptr;
        
        // ВЫВОД
        if (step <= 10) {
            printf("| %5d | %8.4f | %8.4f | %8.4f | %6.2f | %6.2f | %10.6f |\n",
                   step, *command_ptr, *feedback_ptr, error,
                   model.P_gain, model.I_gain, model.Unit_Delay1);
        }
        else if (step % print_interval == 0) {
            printf("| %5d | %8.4f | %8.4f | %8.4f | %6.2f | %6.2f | %10.6f |\n",
                   step, *command_ptr, *feedback_ptr, error,
                   model.P_gain, model.I_gain, model.Unit_Delay1);
        }
        
        if (data_file) {
            fprintf(data_file, "%d,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f\n",
                    step, time, *command_ptr, *feedback_ptr, error,
                    model.P_gain, model.I_gain, model.Unit_Delay1);
        }
    }
    
    // Выводим последний шаг (если он не был выведен)
    if (step > 10 && step % print_interval != 0 && fabs(error) <= tolerance) {
        printf("| %5d | %8.4f | %8.4f | %8.4f | %6.2f | %6.2f | %10.6f |\n",
               step, *command_ptr, *feedback_ptr, error,
               model.P_gain, model.I_gain, model.Unit_Delay1);
    }
    
    printf("+-------+----------+----------+----------+--------+--------+------------+\n");
    
    printf("\n=== RESULTS ===\n");
    printf("Steps taken: %d\n", step);
    printf("Time elapsed: %.3f\n", time);
    printf("Final error: %.10f\n", error);
    printf("Final output: %.6f (target: %.1f)\n", *feedback_ptr, target);
    
    if (step >= max_steps) {
        printf("WARNING: Max steps limit reached!\n");
    } else {
        printf("SUCCESS: Target reached!\n");
    }
    
    if (data_file) {
        fclose(data_file);
        printf("\nData saved to: ./results/simulation_data.csv\n");
    }
    
    return 0;
}