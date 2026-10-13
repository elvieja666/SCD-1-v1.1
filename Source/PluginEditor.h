#pragma once
#include "PluginProcessor.h"

namespace ui
{
    inline const juce::Colour bg      { 0xff313131 };
    inline const juce::Colour label   { 0xffe6e6e6 };   // nombres (Inter Medium)
    inline const juce::Colour value   { 0xffd2d2d2 };   // valores (Inter Regular)
    inline const juce::Colour caption { 0xffcecece };   // Off / On y opciones de Mode

    // Todas las medidas estan en "pixeles de diseno" tomados de la referencia (AL-1, 1920x678).
    constexpr float emLabel = 15.0f, emValue = 15.0f, emNumber = 15.0f, emTitle = 26.0f;
    constexpr float knobScale = 1.33f;                // perilla de la referencia / perilla generada
    constexpr float windowScale = 0.8f;               // tamano real de la ventana en pantalla

    // Distribucion del boceto del usuario, con las medidas de la referencia (pixeles de diseno)
    constexpr int width = 910, height = 589, sepX = 625, sepTop = 29, sepBottom = 563;
    constexpr float midScale = 1.55f;                   // Polarity y Mode se ven mas grandes, como en el boceto
    constexpr int depthX = 180, depthLabelY = 127, depthY = 295;
    constexpr float depthAreaH = 326.0f;
    constexpr int polX = 453, polLabelY = 54, polY = 145, polTextH = 18;
    constexpr float polAreaH = 164.0f;
    constexpr int modeX = 453, modeTitleY = 333;
    constexpr float modeTitleToDisc = 107.0f;
    constexpr int topLabelY = 54;                       // nombre del fader
    constexpr int faderX = 715, faderTop = 69, faderLen = 471, faderTextH = 44;
    constexpr int meterX = 820, meterTopY = 81, meterBottomY = 528;   // centros del primer y ultimo segmento
    constexpr int btnX[2] = { 96, 276 };                // Key Listen, Depth Bypass
    constexpr int btnLabelY = 467;
    constexpr int ringHalf = 245, flatHalf = 95;        // tamano de las imagenes generadas
}

// Tipografia (Inter, licencia OFL) y estilo de perillas de la referencia
class RefLookAndFeel : public juce::LookAndFeel_V4
{
public:
    RefLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float minSliderPos, float maxSliderPos, juce::Slider::SliderStyle, juce::Slider&) override;
    juce::Font getLabelFont (juce::Label&) override { return regularFont (ui::emValue); }
    void drawCapText (juce::Graphics&, const juce::String&, float x, float capCentreY, float em, bool medium,
                      juce::Colour, juce::Justification) const;

    juce::Image ringImg, flatImg, modeHalfImg, modeWholeImg;

    juce::Font regularFont (float em) const { return juce::Font (juce::FontOptions (regular).withHeight (em * 1.21f)); }
    juce::Font mediumFont  (float em) const { return juce::Font (juce::FontOptions (medium).withHeight (em * 1.21f)); }

    static juce::Image makeRingKnobImage();
    static juce::Image makeFlatKnobImage();
    static juce::Image makeModeImage (float angleDeg);

private:
    juce::Typeface::Ptr regular, medium;
};

// Perilla: el doble clic abre la caja para escribir el valor exacto con el teclado
class KnobSlider : public juce::Slider
{
public:
    // el area de dibujo es mas ancha que la perilla (para las sombras): solo responde sobre el circulo
    float hitCy = -1.0f, hitR = 0.0f;
    bool hitTest (int x, int y) override
    {
        if (hitCy < 0.0f)
            return juce::Slider::hitTest (x, y);
        const float dx = (float) x - (float) getWidth() * 0.5f, dy = (float) y - hitCy;
        return dx * dx + dy * dy < hitR * hitR;
    }

    void mouseDoubleClick (const juce::MouseEvent&) override
    {
        if (isEnabled())
            showTextBox();
    }
};

// Boton cuadrado plano (Depth Bypass, Key Listen)
class PushButton : public juce::Component
{
public:
    PushButton (juce::RangedAudioParameter&, const juce::String& title, RefLookAndFeel&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    juce::String title;
    RefLookAndFeel& laf;
    bool on = false;
    juce::ParameterAttachment attachment;
};

// Interruptor chico de 2 posiciones (Mode: Half arriba, Whole abajo)
class MiniToggle : public juce::Component
{
public:
    MiniToggle (juce::RangedAudioParameter&, const juce::String& title, const juce::String& top, const juce::String& bottom, RefLookAndFeel&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    juce::String title, topText, bottomText;
    RefLookAndFeel& laf;
    bool bottomOn = false;
    juce::ParameterAttachment attachment;
};

// Medidor digital de nivel de salida (segmentos como en la referencia): +6 dB arriba, -inf abajo, 0,75 dB por segmento
class GrMeter : public juce::Component
{
public:
    explicit GrMeter (RefLookAndFeel& l) : laf (l) { setInterceptsMouseClicks (false, false); }
    void setLevels (float levelDb, float peakDb)
    {
        if (levelDb != level || peakDb != peak) { level = levelDb; peak = peakDb; repaint(); }
    }
    void paint (juce::Graphics&) override;

private:
    RefLookAndFeel& laf;
    float level = -100.0f, peak = -100.0f;
};

class PhantomAsymEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit PhantomAsymEditor (PhantomAsymProcessor&);
    ~PhantomAsymEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    KnobSlider depthKnob, polarityKnob, outputKnob;

    // para las imagenes de prueba: fija el medidor y deja de leer el procesador
    void previewMeter (float levelDb, float peakDb) { preview = true; meter.setLevels (levelDb, peakDb); }

private:
    using APVTS = juce::AudioProcessorValueTreeState;

    PhantomAsymProcessor& proc;
    RefLookAndFeel laf;

    MiniToggle modeToggle;
    GrMeter meter;
    float meterLevel = -100.0f, meterPeak = -100.0f;
    double peakHoldUntil = 0.0;
    bool preview = false;
    void timerCallback() override;
    PushButton bypassButton, listenButton;

    std::unique_ptr<APVTS::SliderAttachment> depthAtt, polarityAtt, outputAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PhantomAsymEditor)
};
