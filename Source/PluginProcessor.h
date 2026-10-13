#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

class PhantomAsymProcessor : public juce::AudioProcessor
{
public:
    PhantomAsymProcessor();
    ~PhantomAsymProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "SCD-1"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float> outMeterDb { -100.0f };   // pico de salida del ultimo bloque (dBFS) para el medidor


private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    float controlState = 0.0f;
    bool cycleOn = false;        // el ciclo actual esta silenciado
    bool nextDuck = true;        // el proximo ciclo se silencia (alterna)
    bool wasPositive = false;    // detector de ciclos con histeresis
    float cyclePeak = 0.0f;      // peak del cycle en curso (|sidechain|)
    float prevPeak = 0.0f;       // peak del cycle anterior
    int quietCount = 0;
    float listenState = 0.0f;                 // 1 = Key Listen activo (suavizado)
    float bypassState = 1.0f;                 // 1 = efecto activo, 0 = bypass (suavizado)
    std::vector<uint8_t> activeHistory;       // para desplazar la fase de la ventana
    int histPos = 0;
    float periodSamples = 960.0f;             // periodo estimado del sidechain
    int samplesSinceRise = 0;          // muestras seguidas sin señal en el sidechain
    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PhantomAsymProcessor)
};
