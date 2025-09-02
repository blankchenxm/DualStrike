#include <stdio.h>
#include <math.h>

#define INPUT_DIM 24
#define HIDDEN_DIM 64
#define OUTPUT_DIM 49

// Declare weights and biases (defined in another file)
extern float W1[HIDDEN_DIM][INPUT_DIM];
extern float b1[HIDDEN_DIM];
extern float W2[OUTPUT_DIM][HIDDEN_DIM];
extern float b2[OUTPUT_DIM];

// ReLU activation function
float relu(float x) {
    return x > 0 ? x : 0;
}

// Softmax activation function (optional)
void softmax(float* x, int len) {
    float max = x[0];
    for (int i = 1; i < len; i++) if (x[i] > max) max = x[i];

    float sum = 0.0f;
    for (int i = 0; i < len; i++) {
        x[i] = expf(x[i] - max);
        sum += x[i];
    }
    for (int i = 0; i < len; i++) x[i] /= sum;
}

// Neural network inference function
void predict_keypress(float input[INPUT_DIM], int* predicted_label, float* confidence) {
    float hidden[HIDDEN_DIM] = {0};
    float output[OUTPUT_DIM] = {0};

    // First layer: Linear + ReLU
    for (int i = 0; i < HIDDEN_DIM; i++) {
        hidden[i] = b1[i];
        for (int j = 0; j < INPUT_DIM; j++) {
            hidden[i] += W1[i][j] * input[j];
        }
        hidden[i] = relu(hidden[i]);
    }

    // Second layer: Linear
    for (int i = 0; i < OUTPUT_DIM; i++) {
        output[i] = b2[i];
        for (int j = 0; j < HIDDEN_DIM; j++) {
            output[i] += W2[i][j] * hidden[j];
        }
    }

    // Apply softmax activation (optional)
    softmax(output, OUTPUT_DIM);

    // Find the class with maximum probability
    int max_index = 0;
    float max_prob = output[0];
    for (int i = 1; i < OUTPUT_DIM; i++) {
        if (output[i] > max_prob) {
            max_prob = output[i];
            max_index = i;
        }
    }

    *predicted_label = max_index;
    *confidence = max_prob;
}
