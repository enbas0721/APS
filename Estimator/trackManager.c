#include <stdio.h>
#include <string.h>
#include <math.h>
#include "recordManager.h"
#include "trackManager.h"
#include "thermo.h"
#include <limits.h>

void write_result(char * filename, double * time, double * distances, double * ideal, double * c_time, int size){
    FILE *fp;
    fp = fopen(filename, "w");
    int n;
    fprintf(fp, "Time,");
    for (n = 0; n < size; n++){
        fprintf(fp, "%lf,", time[n]);
    }
    fprintf(fp, "%lf\n", time[size]);

    fprintf(fp, "Distance,");
    for (int n = 0; n < size; n++){
        fprintf(fp, "%lf,", distances[n]);
    }
    fprintf(fp, "%lf\n", distances[size]);

    fprintf(fp, "Ideal Time,");
    for (n = 0; n < size; n++){
        fprintf(fp, "%lf,", ideal[n]);
    }
    fprintf(fp, "%lf\n", ideal[size]);

    fprintf(fp, "Calibration Time,");
    fprintf(fp, "%lf\n", c_time[0]);
}
  
double sound_speed(double temperature){
    return (331.5 + (0.61 * temperature));
}

void make_chirp_wave(int16_t* g){
    int n;
	double t;
    int f0 = INIT_FREQ;
    int f1 = FINAL_FREQ;
    int size = SIGNAL_L*SMPL;
    int flag = 0;
    int16_t value;
	for (n = 0; n < size; n++)
	{
        t = (double)n/SMPL;
        value = (int)(sin(2*M_PI * t * (f0 + ((f1-f0)/(2*SIGNAL_L))*t)));
        g[n] = value;
	}
}

void get_input_wave(int16_t* g, char * filename){
    FILE *fp;
    fp = fopen(filename, "rt");
    for (int i = 0; i < CRSS_WNDW_SIZ; i++)
    {
        fscanf(fp, "%d\n", &g[i]);   
    }
}

void cross_correlation(double* fai, int16_t* data, int16_t* ideal_sig, int checking_index){
    int i, j, tau;
    double var_x = 0, var_y = 0;
    int first_index = checking_index - CRSS_WNDW_SIZ*2;
    for (i = 0; i < CRSS_WNDW_SIZ; i++)
    {
        fai[i] = 0;
        var_x += data[i]*data[i];
        var_y += ideal_sig[i]*ideal_sig[i];
    }
    var_x = sqrt(var_x);
    var_y = sqrt(var_y);
    for (tau = 0; tau < CRSS_WNDW_SIZ; tau++)
    {
        for (j = 0; j < CRSS_WNDW_SIZ; j++)
        {   
            // fai[tau] += ((data[first_index + j + tau] * ideal_sig[j])/(CRSS_WNDW_SIZ * var_x * var_y));
            fai[tau] += ((data[first_index + j + tau] * ideal_sig[j]));

            // if(data[first_index + j + tau] == ideal_sig[j]){
            //     printf("tau:%d fai[141]:%e\n",tau,fai[tau]);
            //     printf("data:%ld ideal_sig:%ld\n",data[first_index + j + tau],ideal_sig[j]);
            // }
        }
    }
}

int get_max_index(double* S, size_t size){
    int max_index = 0, i;
    double max_value = 0.0;
    for (i = 0; i < size; i++)
    {
        if(S[i] > max_value){
            max_value = S[i];
            max_index = i;
        }
    }
    return max_index;
}

void* track_start(record_info *info)
{
    int phase = 1;
    int status = 1;
    int calibration_count = 0;
    double calibration_value = 0.1;

    double initial_pos = INIT_POS;
    
    int received_num = 0;
    int checking_index = 0;
    double current_time = 0.0;
    double start_time = 0.0;
    int start_sample = 0;
    double epsilon = 0.01;
    
    int threshold = 200;

    double temperature = 20.0;
    double v = sound_speed(temperature);

    double propagation_time = 0.0;
    double distance = 0.0;

    int log_index = 0;
    double distances[10000];
    double received_time[10000];
    double ideal_received_time[10000];
    double cal_received_time[3];

    int16_t* ideal_signal1;
    int16_t* ideal_signal2;
    int16_t* ideal_signal3;
    char input_wave_path1[64];
    char input_wave_path2[64];
    char input_wave_path3[64];
    strcpy(input_wave_path1, "source/input_wave1.csv");
    strcpy(input_wave_path2, "source/input_wave2.csv");
    strcpy(input_wave_path3, "source/input_wave3.csv");
    ideal_signal1 = (int16_t*)malloc(CRSS_WNDW_SIZ*sizeof(int16_t));
    ideal_signal2 = (int16_t*)malloc(CRSS_WNDW_SIZ*sizeof(int16_t));
    ideal_signal3 = (int16_t*)malloc(CRSS_WNDW_SIZ*sizeof(int16_t));
    get_input_wave(ideal_signal1,input_wave_path1);
    get_input_wave(ideal_signal2,input_wave_path2);
    get_input_wave(ideal_signal3,input_wave_path3);
    
    double* cross_correlation_result;
    cross_correlation_result = calloc((CRSS_WNDW_SIZ), sizeof(double));

    int max_index = 0;

    while((info->flag) || (checking_index < info->last_index))
    {
        if (info->last_index > checking_index){
            current_time = (double)checking_index / (double)SMPL;
            switch(phase){
                case 1:
                    // 信号受信判定
                    if (status)
                    {
                        printf("Waiting signal...\n");
                        status = 0;
                    }
                    if (info->record_data[checking_index] > threshold){
                        cal_received_time[calibration_count] = current_time;
                        // temperature = temp_measure(temperature);
                        v = sound_speed(temperature);
                        start_sample = checking_index - (int)(SMPL*((double)initial_pos/(double)v));
                        start_time = current_time - (initial_pos/v);
                        checking_index = start_sample + (SMPL*1.2);
                        phase = 2;
                        status = 1;
                    }else{
                        checking_index += 1;
                    }
                    break;
                case 2:
                    // 位置推定処理
                    if (status)
                    {
                        printf("Estimation started...\n");
                        status = 0;
                    }
                    max_index = 0;
                    cross_correlation(cross_correlation_result, info->record_data, ideal_signal1, checking_index);
                    max_index += get_max_index(cross_correlation_result, CRSS_WNDW_SIZ);
                    cross_correlation(cross_correlation_result, info->record_data, ideal_signal2, checking_index);
                    max_index += get_max_index(cross_correlation_result, CRSS_WNDW_SIZ);
                    cross_correlation(cross_correlation_result, info->record_data, ideal_signal3, checking_index);
                    max_index += get_max_index(cross_correlation_result, CRSS_WNDW_SIZ);
                    max_index /= 3;
                    propagation_time = (double)max_index/(double)SMPL;
                    // temperature = temp_measure(temperature);
                    v = sound_speed(temperature);
                    distance = propagation_time * v;
                    printf("受信回数: %d {回}\n",log_index);
                    printf("サンプル: %d \n",max_index);
                    printf("推定距離: %lf {m}\n", distance);
                    printf("--------------------\n");
                    
                    distances[log_index] = distance;
                    received_time[log_index] = ((checking_index - CRSS_WNDW_SIZ*2)/SMPL) + propagation_time;
                    ideal_received_time[log_index] = ((checking_index - CRSS_WNDW_SIZ*2)/SMPL);
                    log_index += 1; 
                    checking_index += SMPL;
                    break;
                default:
                    printf("Error: Non double * ideal, -existent phase\n");
            }
        }
    }
    
    char filename[64];
    strcpy(filename,info->filename);
    strcat(filename, ".csv");
    write_result(filename, received_time, distances, ideal_received_time, cal_received_time, log_index-1);

    free(ideal_signal1);
    free(ideal_signal2);
    free(ideal_signal3);
    free(cross_correlation_result);
}

