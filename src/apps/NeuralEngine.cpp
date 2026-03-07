/**
 * NeuralEngine.cpp
 * Tiny-Backprop MLP implementation for NumOS Neural Lab.
 * SGD with Momentum and optional L2 Regularization.
 * All math in float for ESP32-S3 FPU acceleration.
 */

#include "NeuralEngine.h"

#ifdef ARDUINO
#include <esp_heap_caps.h>
#endif

// ── Portable PSRAM allocation ──
static float* psramAlloc(size_t count) {
#ifdef ARDUINO
    float* p = (float*)heap_caps_malloc(count * sizeof(float), MALLOC_CAP_SPIRAM);
#else
    float* p = new (std::nothrow) float[count];
#endif
    if (p) memset(p, 0, count * sizeof(float));
    return p;
}

static void psramFree(float* p) {
#ifdef ARDUINO
    if (p) heap_caps_free(p);
#else
    delete[] p;
#endif
}

// ═══════════════════════════════════════════════════════════════════════════════
// Construction / Destruction
// ═══════════════════════════════════════════════════════════════════════════════

NeuralEngine::NeuralEngine()
    : _numLayers(0)
    , _activation(Activation::SIGMOID)
    , _lr(0.1f)
    , _momentum(0.9f)
    , _l2Lambda(0.0f)
{
    memset(_layerSizes, 0, sizeof(_layerSizes));
    memset(_weights, 0, sizeof(_weights));
    memset(_biases, 0, sizeof(_biases));
    memset(_vWeights, 0, sizeof(_vWeights));
    memset(_vBiases, 0, sizeof(_vBiases));
    memset(_outputs, 0, sizeof(_outputs));
    memset(_preAct, 0, sizeof(_preAct));
    memset(_deltas, 0, sizeof(_deltas));
}

NeuralEngine::~NeuralEngine() {
    freeAll();
}

// ═══════════════════════════════════════════════════════════════════════════════
// Configuration
// ═══════════════════════════════════════════════════════════════════════════════

void NeuralEngine::setTopology(const int* sizes, int numLayers) {
    freeAll();
    _numLayers = (numLayers > NE_MAX_LAYERS) ? NE_MAX_LAYERS : numLayers;
    for (int i = 0; i < _numLayers; i++) {
        _layerSizes[i] = (sizes[i] > NE_MAX_NEURONS) ? NE_MAX_NEURONS : sizes[i];
    }
    // Allocate weight matrices between consecutive layers
    for (int l = 0; l < _numLayers - 1; l++) {
        allocLayer(l);
    }
}

void NeuralEngine::setActivation(Activation act) {
    _activation = act;
}

void NeuralEngine::setLearningRate(float lr) {
    _lr = lr;
}

void NeuralEngine::setMomentum(float gamma) {
    _momentum = gamma;
}

void NeuralEngine::setL2Lambda(float lambda) {
    _l2Lambda = lambda;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Memory Management
// ═══════════════════════════════════════════════════════════════════════════════

void NeuralEngine::allocLayer(int l) {
    int fromSize = _layerSizes[l];
    int toSize   = _layerSizes[l + 1];
    freeLayer(l);
    _weights[l]  = psramAlloc(toSize * fromSize);
    _biases[l]   = psramAlloc(toSize);
    _vWeights[l] = psramAlloc(toSize * fromSize);
    _vBiases[l]  = psramAlloc(toSize);
}

void NeuralEngine::freeLayer(int l) {
    if (_weights[l])  { psramFree(_weights[l]);  _weights[l]  = nullptr; }
    if (_biases[l])   { psramFree(_biases[l]);   _biases[l]   = nullptr; }
    if (_vWeights[l]) { psramFree(_vWeights[l]); _vWeights[l] = nullptr; }
    if (_vBiases[l])  { psramFree(_vBiases[l]);  _vBiases[l]  = nullptr; }
}

void NeuralEngine::freeAll() {
    for (int l = 0; l < NE_MAX_LAYERS - 1; l++) {
        freeLayer(l);
    }
    _numLayers = 0;
    memset(_layerSizes, 0, sizeof(_layerSizes));
}

// ═══════════════════════════════════════════════════════════════════════════════
// Weight Initialization — He for ReLU, Xavier for Sigmoid/Tanh
// ═══════════════════════════════════════════════════════════════════════════════

float NeuralEngine::randomNormal(float stddev) {
    // Box-Muller transform for normal distribution
    float u1 = ((float)rand() / RAND_MAX);
    float u2 = ((float)rand() / RAND_MAX);
    if (u1 < 1e-7f) u1 = 1e-7f;
    float z = sqrtf(-2.0f * logf(u1)) * cosf(2.0f * (float)M_PI * u2);
    return z * stddev;
}

void NeuralEngine::initWeights() {
    for (int l = 0; l < _numLayers - 1; l++) {
        int fromSize = _layerSizes[l];
        int toSize   = _layerSizes[l + 1];
        if (!_weights[l] || !_biases[l]) continue;

        // He init for ReLU, Xavier for Sigmoid/Tanh
        float stddev;
        if (_activation == Activation::RELU) {
            stddev = sqrtf(2.0f / (float)fromSize);       // He
        } else {
            stddev = sqrtf(2.0f / (float)(fromSize + toSize)); // Xavier
        }

        for (int i = 0; i < toSize * fromSize; i++) {
            _weights[l][i] = randomNormal(stddev);
        }
        for (int i = 0; i < toSize; i++) {
            _biases[l][i] = 0.0f;
        }

        // Zero momentum buffers
        if (_vWeights[l])
            memset(_vWeights[l], 0, toSize * fromSize * sizeof(float));
        if (_vBiases[l])
            memset(_vBiases[l], 0, toSize * sizeof(float));
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Activation Functions
// ═══════════════════════════════════════════════════════════════════════════════

float NeuralEngine::activate(float x, Activation act) {
    switch (act) {
        case Activation::SIGMOID: {
            if (x > 15.0f) return 1.0f;
            if (x < -15.0f) return 0.0f;
            return 1.0f / (1.0f + expf(-x));
        }
        case Activation::RELU:
            return (x > 0.0f) ? x : 0.0f;
        case Activation::TANH: {
            if (x > 10.0f) return 1.0f;
            if (x < -10.0f) return -1.0f;
            return tanhf(x);
        }
        default:
            return x;
    }
}

float NeuralEngine::activateDerivative(float output, float preact, Activation act) {
    switch (act) {
        case Activation::SIGMOID:
            return output * (1.0f - output);
        case Activation::RELU:
            return (preact > 0.0f) ? 1.0f : 0.0f;
        case Activation::TANH:
            return 1.0f - output * output;
        default:
            return 1.0f;
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Forward Pass
// ═══════════════════════════════════════════════════════════════════════════════

void NeuralEngine::forward(const float* input) {
    if (_numLayers < 2) return;

    // Copy input to layer 0 outputs
    for (int i = 0; i < _layerSizes[0]; i++) {
        _outputs[0][i] = input[i];
        _preAct[0][i]  = input[i];
    }

    // Propagate through each layer
    for (int l = 0; l < _numLayers - 1; l++) {
        int fromSize = _layerSizes[l];
        int toSize   = _layerSizes[l + 1];
        if (!_weights[l] || !_biases[l]) continue;

        for (int j = 0; j < toSize; j++) {
            float sum = _biases[l][j];
            for (int i = 0; i < fromSize; i++) {
                sum += _weights[l][j * fromSize + i] * _outputs[l][i];
            }
            _preAct[l + 1][j] = sum;

            // Output layer: use sigmoid for classification, linear for regression
            // Hidden layers: use selected activation
            bool isOutput = (l == _numLayers - 2);
            if (isOutput && _activation == Activation::RELU) {
                // For regression with ReLU hidden layers, use sigmoid output
                _outputs[l + 1][j] = activate(sum, Activation::SIGMOID);
            } else {
                _outputs[l + 1][j] = activate(sum, _activation);
            }
        }
    }
}

const float* NeuralEngine::getOutput() const {
    if (_numLayers < 2) return nullptr;
    return _outputs[_numLayers - 1];
}

float NeuralEngine::getOutputAt(int i) const {
    if (_numLayers < 2 || i >= _layerSizes[_numLayers - 1]) return 0.0f;
    return _outputs[_numLayers - 1][i];
}

int NeuralEngine::getOutputSize() const {
    if (_numLayers < 2) return 0;
    return _layerSizes[_numLayers - 1];
}

// ═══════════════════════════════════════════════════════════════════════════════
// Backpropagation
// ═══════════════════════════════════════════════════════════════════════════════

void NeuralEngine::backward(const float* target) {
    if (_numLayers < 2) return;
    int outLayer = _numLayers - 1;
    int outSize  = _layerSizes[outLayer];

    // ── Output layer deltas ──
    for (int j = 0; j < outSize; j++) {
        float o = _outputs[outLayer][j];
        float err = o - target[j];

        // Derivative depends on what activation was used for output
        bool reLUHidden = (_activation == Activation::RELU);
        Activation outAct = reLUHidden ? Activation::SIGMOID : _activation;
        _deltas[outLayer][j] = err * activateDerivative(o, _preAct[outLayer][j], outAct);
    }

    // ── Hidden layer deltas (backpropagate) ──
    for (int l = outLayer - 1; l >= 1; l--) {
        int curSize  = _layerSizes[l];
        int nextSize = _layerSizes[l + 1];
        if (!_weights[l]) continue;

        for (int i = 0; i < curSize; i++) {
            float sum = 0.0f;
            for (int j = 0; j < nextSize; j++) {
                sum += _deltas[l + 1][j] * _weights[l][j * curSize + i];
            }
            _deltas[l][i] = sum * activateDerivative(
                _outputs[l][i], _preAct[l][i], _activation);
        }
    }
}

void NeuralEngine::updateWeights() {
    for (int l = 0; l < _numLayers - 1; l++) {
        int fromSize = _layerSizes[l];
        int toSize   = _layerSizes[l + 1];
        if (!_weights[l] || !_biases[l]) continue;

        for (int j = 0; j < toSize; j++) {
            for (int i = 0; i < fromSize; i++) {
                int idx = j * fromSize + i;
                float grad = _deltas[l + 1][j] * _outputs[l][i];

                // L2 regularization (weight decay)
                if (_l2Lambda > 0.0f) {
                    grad += _l2Lambda * _weights[l][idx];
                }

                // SGD with Momentum: v = γ*v + lr*grad; w -= v
                if (_vWeights[l]) {
                    _vWeights[l][idx] = _momentum * _vWeights[l][idx] + _lr * grad;
                    _weights[l][idx] -= _vWeights[l][idx];
                } else {
                    _weights[l][idx] -= _lr * grad;
                }
            }

            // Bias update (no L2 on biases)
            float bGrad = _deltas[l + 1][j];
            if (_vBiases[l]) {
                _vBiases[l][j] = _momentum * _vBiases[l][j] + _lr * bGrad;
                _biases[l][j] -= _vBiases[l][j];
            } else {
                _biases[l][j] -= _lr * bGrad;
            }
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Training
// ═══════════════════════════════════════════════════════════════════════════════

float NeuralEngine::trainSingleSample(const float* input, const float* target) {
    forward(input);
    backward(target);
    updateWeights();

    // Compute MSE for this sample
    int outSize = _layerSizes[_numLayers - 1];
    float mse = 0.0f;
    for (int i = 0; i < outSize; i++) {
        float err = _outputs[_numLayers - 1][i] - target[i];
        mse += err * err;
    }
    return mse / outSize;
}

float NeuralEngine::trainEpoch(const TrainSample* samples, int count) {
    if (count == 0 || _numLayers < 2) return 0.0f;

    float totalMSE = 0.0f;
    for (int s = 0; s < count; s++) {
        totalMSE += trainSingleSample(samples[s].inputs, samples[s].targets);
    }
    return totalMSE / count;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Accessors
// ═══════════════════════════════════════════════════════════════════════════════

float NeuralEngine::getWeight(int layer, int to, int from) const {
    if (layer < 0 || layer >= _numLayers - 1) return 0.0f;
    if (!_weights[layer]) return 0.0f;
    int fromSize = _layerSizes[layer];
    return _weights[layer][to * fromSize + from];
}

float NeuralEngine::getNeuronOutput(int layer, int neuron) const {
    if (layer < 0 || layer >= _numLayers) return 0.0f;
    if (neuron < 0 || neuron >= _layerSizes[layer]) return 0.0f;
    return _outputs[layer][neuron];
}

int NeuralEngine::getParamCount() const {
    int total = 0;
    for (int l = 0; l < _numLayers - 1; l++) {
        total += _layerSizes[l] * _layerSizes[l + 1]; // weights
        total += _layerSizes[l + 1];                    // biases
    }
    return total;
}

float NeuralEngine::computeAccuracy(TrainSample const* samples, int count) {
    if (count == 0 || _numLayers < 2) return 0.0f;
    int correct = 0;
    for (int i = 0; i < count; i++) {
        forward(samples[i].inputs);
        float out = getOutputAt(0);
        float predicted = (out >= 0.5f) ? 1.0f : 0.0f;
        if (fabsf(predicted - samples[i].targets[0]) < 0.1f) {
            correct++;
        }
    }
    return (float)correct / (float)count * 100.0f;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Dynamic Topology Modification
// ═══════════════════════════════════════════════════════════════════════════════

void NeuralEngine::addNeuronToLayer(int layer) {
    if (layer <= 0 || layer >= _numLayers - 1) return;  // only hidden layers
    if (_layerSizes[layer] >= NE_MAX_NEURONS) return;

    _layerSizes[layer]++;

    // Reallocate connections to/from this layer
    if (layer > 0) allocLayer(layer - 1);
    if (layer < _numLayers - 1) allocLayer(layer);

    initWeights();  // re-randomize all weights
}

void NeuralEngine::removeNeuronFromLayer(int layer) {
    if (layer <= 0 || layer >= _numLayers - 1) return;  // only hidden layers
    if (_layerSizes[layer] <= 1) return;

    _layerSizes[layer]--;

    // Reallocate connections to/from this layer
    if (layer > 0) allocLayer(layer - 1);
    if (layer < _numLayers - 1) allocLayer(layer);

    initWeights();  // re-randomize all weights
}

// ═══════════════════════════════════════════════════════════════════════════════
// Model Persistence (Serialize / Deserialize)
// ═══════════════════════════════════════════════════════════════════════════════

int NeuralEngine::serialize(uint8_t* buf, int maxBytes) const {
    if (_numLayers < 2) return 0;

    int offset = 0;
    auto writeVal = [&](const void* src, int sz) -> bool {
        if (offset + sz > maxBytes) return false;
        memcpy(buf + offset, src, sz);
        offset += sz;
        return true;
    };

    // Header: magic "NN" + version 0x0001 (4 bytes total)
    uint32_t magic = 0x4E4E0001; // 'N','N', version 0x0001
    if (!writeVal(&magic, 4)) return 0;

    // Topology
    if (!writeVal(&_numLayers, sizeof(int))) return 0;
    for (int i = 0; i < _numLayers; i++) {
        if (!writeVal(&_layerSizes[i], sizeof(int))) return 0;
    }

    // Activation, LR, momentum, L2
    uint8_t act = (uint8_t)_activation;
    if (!writeVal(&act, 1)) return 0;
    if (!writeVal(&_lr, sizeof(float))) return 0;
    if (!writeVal(&_momentum, sizeof(float))) return 0;
    if (!writeVal(&_l2Lambda, sizeof(float))) return 0;

    // Weights and biases
    for (int l = 0; l < _numLayers - 1; l++) {
        int wSize = _layerSizes[l] * _layerSizes[l + 1];
        if (_weights[l]) {
            if (!writeVal(_weights[l], wSize * sizeof(float))) return 0;
        }
        if (_biases[l]) {
            if (!writeVal(_biases[l], _layerSizes[l + 1] * sizeof(float))) return 0;
        }
    }

    return offset;
}

bool NeuralEngine::deserialize(const uint8_t* buf, int bytes) {
    int offset = 0;
    auto readVal = [&](void* dst, int sz) -> bool {
        if (offset + sz > bytes) return false;
        memcpy(dst, buf + offset, sz);
        offset += sz;
        return true;
    };

    // Magic
    uint32_t magic = 0;
    if (!readVal(&magic, 4)) return false;
    if (magic != 0x4E4E0001) return false;

    // Topology
    int nl = 0;
    if (!readVal(&nl, sizeof(int))) return false;
    if (nl < 2 || nl > NE_MAX_LAYERS) return false;

    int sizes[NE_MAX_LAYERS];
    for (int i = 0; i < nl; i++) {
        if (!readVal(&sizes[i], sizeof(int))) return false;
        if (sizes[i] < 1 || sizes[i] > NE_MAX_NEURONS) return false;
    }

    // Rebuild topology
    setTopology(sizes, nl);

    // Activation, LR, momentum, L2
    uint8_t act = 0;
    if (!readVal(&act, 1)) return false;
    if (act >= (uint8_t)Activation::COUNT) return false;
    _activation = (Activation)act;
    if (!readVal(&_lr, sizeof(float))) return false;
    if (!readVal(&_momentum, sizeof(float))) return false;
    if (!readVal(&_l2Lambda, sizeof(float))) return false;

    // Weights and biases
    for (int l = 0; l < _numLayers - 1; l++) {
        int wSize = _layerSizes[l] * _layerSizes[l + 1];
        if (_weights[l]) {
            if (!readVal(_weights[l], wSize * sizeof(float))) return false;
        }
        if (_biases[l]) {
            if (!readVal(_biases[l], _layerSizes[l + 1] * sizeof(float))) return false;
        }
    }

    // Zero momentum buffers after load
    for (int l = 0; l < _numLayers - 1; l++) {
        int fromSize = _layerSizes[l];
        int toSize   = _layerSizes[l + 1];
        if (_vWeights[l]) memset(_vWeights[l], 0, toSize * fromSize * sizeof(float));
        if (_vBiases[l])  memset(_vBiases[l], 0, toSize * sizeof(float));
    }

    return true;
}
