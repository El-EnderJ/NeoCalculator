/**
 * NeuralEngine.h
 * Tiny-Backprop MLP engine for NumOS Neural Lab.
 * Supports up to 5 layers (Input, 3 Hidden, Output).
 * Activations: Sigmoid, ReLU, Tanh.
 * Training via manual backpropagation with gradient descent.
 *
 * All math uses float (ESP32-S3 FPU accelerated).
 * Weight/bias/delta matrices allocated in PSRAM on Arduino.
 */

#pragma once

#ifdef ARDUINO
#include <Arduino.h>
#else
#include "hal/ArduinoCompat.h"
#endif

#include <cstdint>
#include <cstring>
#include <cmath>
#include <cstdlib>

// ── Maximum network topology ──
static constexpr int NE_MAX_LAYERS  = 5;   // Input + 3 Hidden + Output
static constexpr int NE_MAX_NEURONS = 16;  // max neurons per layer

// ── Activation function types ──
enum class Activation : uint8_t {
    SIGMOID = 0,
    RELU,
    TANH,
    COUNT
};

// ── Training scenario ──
enum class Scenario : uint8_t {
    XOR = 0,
    CLASSIFIER,
    SINE_REGRESSION,
    COUNT
};

// ── A single training sample ──
struct TrainSample {
    float inputs[NE_MAX_NEURONS];
    float targets[NE_MAX_NEURONS];
};

/**
 * NeuralEngine — Fully connected MLP with backpropagation.
 *
 * Layer layout:  layerSizes[0] = input, ..., layerSizes[numLayers-1] = output.
 * Weights[l] : matrix of size layerSizes[l+1] x layerSizes[l]
 * Biases[l]  : vector of size layerSizes[l+1]
 */
class NeuralEngine {
public:
    NeuralEngine();
    ~NeuralEngine();

    // ── Configuration ──
    void setTopology(const int* sizes, int numLayers);
    void setActivation(Activation act);
    void setLearningRate(float lr);

    // ── Initialization ──
    void initWeights();        // Xavier/He initialization
    void freeAll();            // Release all allocations

    // ── Forward pass ──
    void forward(const float* input);
    const float* getOutput() const;
    float getOutputAt(int i) const;
    int   getOutputSize() const;

    // ── Training ──
    float trainEpoch(const TrainSample* samples, int count);  // returns MSE
    float trainSingleSample(const float* input, const float* target); // returns sample MSE

    // ── Accessors ──
    int   getNumLayers() const { return _numLayers; }
    int   getLayerSize(int l) const { return _layerSizes[l]; }
    float getWeight(int layer, int to, int from) const;
    float getNeuronOutput(int layer, int neuron) const;
    Activation getActivation() const { return _activation; }
    float getLearningRate() const { return _lr; }

    // ── Topology modification ──
    void addNeuronToLayer(int layer);
    void removeNeuronFromLayer(int layer);

    // ── Activation math (public for decision boundary) ──
    static float activate(float x, Activation act);

private:
    // ── Network topology ──
    int _numLayers;
    int _layerSizes[NE_MAX_LAYERS];

    // ── Activation ──
    Activation _activation;
    float _lr;  // learning rate

    // ── Weight matrices: _weights[l][to * fromSize + from] ──
    // l = 0..(numLayers-2), connects layer l -> layer l+1
    float* _weights[NE_MAX_LAYERS - 1];
    float* _biases[NE_MAX_LAYERS - 1];

    // ── Neuron activations (outputs after activation function) ──
    float _outputs[NE_MAX_LAYERS][NE_MAX_NEURONS];

    // ── Pre-activation values (z = W*a + b, before activation) ──
    float _preAct[NE_MAX_LAYERS][NE_MAX_NEURONS];

    // ── Deltas for backpropagation ──
    float _deltas[NE_MAX_LAYERS][NE_MAX_NEURONS];

    // ── Internal helpers ──
    void allocLayer(int l);
    void freeLayer(int l);
    static float activateDerivative(float output, float preact, Activation act);
    static float heInit(int fanIn);

    // ── Backpropagation ──
    void backward(const float* target);
    void updateWeights();
};
