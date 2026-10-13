#include "PluginEditor.h"
#include "BinaryData.h"

using namespace juce;

//==============================================================================
RefLookAndFeel::RefLookAndFeel()
{
    regular = Typeface::createSystemTypefaceFor (BinaryData::InterRegular_ttf, (size_t) BinaryData::InterRegular_ttfSize);
    medium  = Typeface::createSystemTypefaceFor (BinaryData::InterMedium_ttf,  (size_t) BinaryData::InterMedium_ttfSize);
    setDefaultSansSerifTypeface (regular);

    ringImg      = makeRingKnobImage();
    flatImg      = makeFlatKnobImage();
    modeHalfImg  = makeModeImage (-43.55f);
    modeWholeImg = makeModeImage (40.0f);

    setColour (Slider::textBoxTextColourId,      ui::value);
    setColour (Slider::textBoxOutlineColourId,   Colours::transparentBlack);
    setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
    setColour (Label::textColourId,              ui::label);
    setColour (TextEditor::backgroundColourId,   Colour (0xff1f1f1f));
    setColour (TextEditor::textColourId,         ui::label);
    setColour (TextEditor::highlightColourId,    Colour (0xff7feeec).withAlpha (0.45f));
    setColour (TextEditor::outlineColourId,      Colour (0xff5a5a5a));
    setColour (TextEditor::focusedOutlineColourId, Colour (0xff7feeec));
    setColour (CaretComponent::caretColourId,    ui::label);
}


//==============================================================================
// Perilla generada por calculo (sin recortes): aro con luz desde arriba-izquierda,
// tapa plana con reborde, marcas y sombras. Los valores se ajustaron midiendo la referencia.
namespace
{
    constexpr int kS = 4;                 // sobremuestreo (nitidez en pantallas retina)
    constexpr int kW = 150, kH = 130;     // tamano logico
    constexpr float kCx = 70.0f, kCy = 60.0f;   // centro dentro de la imagen

    float angTable (const std::vector<std::pair<float, float>>& t, float deg)
    {
        while (deg < -180.0f) deg += 360.0f;
        while (deg >= 180.0f) deg -= 360.0f;
        for (size_t i = 0; i < t.size(); ++i)
        {
            const auto a = t[i];
            const auto b = (i + 1 < t.size()) ? t[i + 1] : std::pair<float, float> { t[0].first + 360.0f, t[0].second };
            if (deg >= a.first && deg <= b.first)
                return a.second + (b.second - a.second) * (deg - a.first) / (b.first - a.first);
        }
        return t.back().second;
    }

    // mascara circular suave (0..1) a escala 1:1, desenfocada con gaussiana separable
    std::vector<float> blurredDisc (int kW, int kH, float kCx, float kCy, float ox, float oy, float radius, float sigma)
    {
        std::vector<float> m ((size_t) (kW * kH)), tmp (m.size());
        for (int y = 0; y < kH; ++y)
            for (int x = 0; x < kW; ++x)
                m[(size_t) (y * kW + x)] = jlimit (0.0f, 1.0f, radius - std::hypot ((float) x + 0.5f - kCx - ox, (float) y + 0.5f - kCy - oy) + 0.5f);

        const int rad = (int) std::ceil (sigma * 3.0f);
        std::vector<float> k ((size_t) (2 * rad + 1)); float sum = 0;
        for (int i = -rad; i <= rad; ++i) { k[(size_t) (i + rad)] = std::exp (-(float) (i * i) / (2.0f * sigma * sigma)); sum += k[(size_t) (i + rad)]; }
        for (auto& v : k) v /= sum;
        for (int y = 0; y < kH; ++y)
            for (int x = 0; x < kW; ++x)
            {
                float s = 0;
                for (int i = -rad; i <= rad; ++i) s += k[(size_t) (i + rad)] * m[(size_t) (y * kW + jlimit (0, kW - 1, x + i))];
                tmp[(size_t) (y * kW + x)] = s;
            }
        for (int y = 0; y < kH; ++y)
            for (int x = 0; x < kW; ++x)
            {
                float s = 0;
                for (int i = -rad; i <= rad; ++i) s += k[(size_t) (i + rad)] * tmp[(size_t) (jlimit (0, kH - 1, y + i) * kW + x)];
                m[(size_t) (y * kW + x)] = s;
            }
        return m;
    }

    float sampleMask (const std::vector<float>& m, int kW, int kH, float fx, float fy)
    {
        fx = jlimit (0.0f, (float) kW - 1.001f, fx - 0.5f);
        fy = jlimit (0.0f, (float) kH - 1.001f, fy - 0.5f);
        const int x0 = (int) fx, y0 = (int) fy; const float tx = fx - (float) x0, ty = fy - (float) y0;
        auto at = [&] (int x, int y) { return m[(size_t) (y * kW + x)]; };
        return (at (x0, y0) * (1 - tx) + at (x0 + 1, y0) * tx) * (1 - ty) + (at (x0, y0 + 1) * (1 - tx) + at (x0 + 1, y0 + 1) * tx) * ty;
    }
}

namespace
{
    struct RingKnob   // perilla grande con aro + tapa (Depth)
    {
        float R, cr, sdx, sdy, ssig, sal, edge, cdx, cdy, csig, cal, rw, rimk;
        int nt; float tw, tg, tl, tv;
    };
    struct FlatKnob   // perilla plana de una pieza (Polarity)
    {
        float R, sdx, sdy, ssig, sal, rimk, rw, rr;
        int nt; float tw, tg, tl, tv;
    };

    const float tickK13[13] = { 1, .85f, .8f, 1, .9f, .9f, 1, .9f, .95f, 1, .6f, .5f, .65f };

    const std::vector<std::pair<float, float>>& ringTab()
    {
        static const std::vector<std::pair<float, float>> t {
            { -180, 42.5f }, { -160, 44.5f }, { -140, 46.5f }, { -120, 48.5f }, { -100, 47 }, { -80, 47.5f }, { -60, 47.5f }, { -40, 41 },
            { -20, 31.5f }, { 0, 20.5f }, { 20, 13.5f }, { 40, 13 }, { 60, 19.5f }, { 80, 29.5f }, { 100, 38.5f }, { 120, 42 }, { 140, 42 }, { 160, 41.5f } };
        return t;
    }
    const std::vector<std::pair<float, float>>& capTab()
    {
        static const std::vector<std::pair<float, float>> t {
            { -180, 84 }, { -160, 87 }, { -140, 90 }, { -120, 84 }, { -100, 79 }, { -80, 67 }, { -60, 61 }, { -40, 57 }, { -20, 53 },
            { 0, 46 }, { 20, 40 }, { 40, 35 }, { 60, 29 }, { 80, 25 }, { 100, 29 }, { 120, 47 }, { 140, 57 }, { 160, 68 } };
        return t;
    }

    Image renderRingKnob (const RingKnob& p, int half, int S)
    {
        const int dim = half * 2; const float c0 = (float) half;
        const float s = p.R / 43.23f;
        const auto ringShadow = blurredDisc (dim, dim, c0, c0, p.sdx, p.sdy, p.R, p.ssig);
        const auto capShadow  = blurredDisc (dim, dim, c0, c0, p.cdx, p.cdy, p.cr, p.csig);

        Image im (Image::ARGB, dim * S, dim * S, true);
        Image::BitmapData bd (im, Image::BitmapData::writeOnly);
        auto sat = [] (float d) { return jlimit (0.0f, 1.0f, d); };

        for (int py = 0; py < dim * S; ++py)
            for (int px = 0; px < dim * S; ++px)
            {
                const float fx = ((float) px + 0.5f) / S, fy = ((float) py + 0.5f) / S;
                const float x = fx - c0, y = fy - c0, r = std::hypot (x, y), th = radiansToDegrees (std::atan2 (y, x));

                float v = 0.0f, A = p.sal * sampleMask (ringShadow, dim, dim, fx, fy);

                const float inRing = sat ((p.R - r) * S + 0.5f);
                v = v * (1 - inRing) + angTable (ringTab(), th) * inRing;
                A = A * (1 - inRing) + inRing;

                const float edge = std::exp (-std::pow ((r - (p.R - 0.3f * s)) / (0.8f * s), 2.0f)) * p.edge;
                v = v * (1 - edge) + 8.0f * edge;
                A = A * (1 - edge) + edge;

                if (r < p.R)
                    v *= 1.0f - p.cal * sampleMask (capShadow, dim, dim, fx, fy);

                const float inCap = sat ((p.cr - r) * S + 0.5f);
                v = v * (1 - inCap) + 50.0f * inCap;
                A = A * (1 - inCap) + inCap;

                if (r < p.cr + 2.0f * s)
                    v += (angTable (capTab(), th) - 50.0f) * std::exp (-std::pow ((r - (p.cr - 1.3f * s)) / p.rw, 2.0f)) * p.rimk;

                // marcas con brillo por marca
                for (int i = 0; i < p.nt; ++i)
                {
                    const float a = degreesToRadians (-135.0f + 270.0f * (float) i / (float) (p.nt - 1) - 90.0f);
                    const float ca = std::cos (a), sa = std::sin (a);
                    const float along = x * ca + y * sa, perp = -x * sa + y * ca;
                    const float m = sat (1.0f - std::abs (perp) / p.tw + 0.5f) * sat ((along - (p.R + p.tg)) * S + 0.5f) * sat (((p.R + p.tg + p.tl) - along) * S + 0.5f);
                    const float pk = p.tv * (p.nt == 13 ? tickK13[i] : 1.0f);
                    v = v * (1 - m) + pk * m;
                    A = A * (1 - m) + m;
                }

                const auto g8 = (uint8) jlimit (0, 255, roundToInt (v));
                bd.setPixelColour (px, py, Colour (g8, g8, g8, (uint8) jlimit (0, 255, roundToInt (A * 255.0f))));
            }
        return im;
    }

    Image renderFlatKnob (const FlatKnob& p, int half, int S)
    {
        const int dim = half * 2; const float c0 = (float) half;
        const auto shadow = blurredDisc (dim, dim, c0, c0, p.sdx, p.sdy, p.R, p.ssig);

        Image im (Image::ARGB, dim * S, dim * S, true);
        Image::BitmapData bd (im, Image::BitmapData::writeOnly);
        auto sat = [] (float d) { return jlimit (0.0f, 1.0f, d); };

        for (int py = 0; py < dim * S; ++py)
            for (int px = 0; px < dim * S; ++px)
            {
                const float fx = ((float) px + 0.5f) / S, fy = ((float) py + 0.5f) / S;
                const float x = fx - c0, y = fy - c0, r = std::hypot (x, y), th = radiansToDegrees (std::atan2 (y, x));

                float v = 0.0f, A = p.sal * sampleMask (shadow, dim, dim, fx, fy);

                const float inside = sat ((p.R - r) * S + 0.5f);
                v = v * (1 - inside) + 48.0f * inside;
                A = A * (1 - inside) + inside;

                if (r < p.R + 3.0f)
                    v += (angTable (capTab(), th) - 48.0f) * std::exp (-std::pow ((r - (p.R - p.rr)) / p.rw, 2.0f)) * p.rimk;

                for (int i = 0; i < p.nt; ++i)
                {
                    const float a = degreesToRadians (-135.0f + 270.0f * (float) i / (float) (p.nt - 1) - 90.0f);
                    const float ca = std::cos (a), sa = std::sin (a);
                    const float along = x * ca + y * sa, perp = -x * sa + y * ca;
                    const float m = sat (1.0f - std::abs (perp) / p.tw + 0.5f) * sat ((along - (p.R + p.tg)) * S + 0.5f) * sat (((p.R + p.tg + p.tl) - along) * S + 0.5f);
                    v = v * (1 - m) + p.tv * (p.nt == 13 ? tickK13[i] : 1.0f) * m;
                    A = A * (1 - m) + m;
                }

                const auto g8 = (uint8) jlimit (0, 255, roundToInt (v));
                bd.setPixelColour (px, py, Colour (g8, g8, g8, (uint8) jlimit (0, 255, roundToInt (A * 255.0f))));
            }
        return im;
    }
}

Image RefLookAndFeel::makeRingKnobImage()
{
    // Depth: perilla grande de la referencia (Input), valores medidos
    const RingKnob p { 134.1f, 100.2f, 29.77f, 16.37f, 25.23f, 0.55f, 0.53f, 16.12f, 8.89f, 4.27f, 0.24f, 4.29f, 1.3f, 13, 1.3f, 3.2f, 9.8f, 190.0f };
    return renderRingKnob (p, ui::ringHalf, 2);
}

Image RefLookAndFeel::makeFlatKnobImage()
{
    // Polarity: perilla pequena plana de la referencia (Attack), valores medidos
    const FlatKnob p { 36.26f, 15.53f, 12.65f, 13.73f, 0.91f, 1.26f, 2.44f, 2.8f, 13, 1.3f / ui::midScale, 3.2f / ui::midScale, 9.8f / ui::midScale, 190.0f };
    return renderFlatKnob (p, ui::flatHalf, 4);
}

// Perilla de Mode generada por calculo: mitad clara, banda central con el puntero y mitad oscura.
Image RefLookAndFeel::makeModeImage (float angleDeg)
{
    constexpr int mW = 100, mH = 100; constexpr float mCx = 50.0f, mCy = 50.0f;
    const float R = 23.11f;
    const auto shadow = blurredDisc (mW, mH, mCx, mCy, 7.79f, 8.47f, R, 5.71f);
    const float ang = degreesToRadians (angleDeg);
    const float ca = std::cos (ang), sa = std::sin (ang);

    Image im (Image::ARGB, mW * kS, mH * kS, true);
    Image::BitmapData bd (im, Image::BitmapData::writeOnly);
    auto sat = [] (float d) { return jlimit (0.0f, 1.0f, d); };

    for (int py = 0; py < mH * kS; ++py)
        for (int px = 0; px < mW * kS; ++px)
        {
            const float fx = ((float) px + 0.5f) / kS, fy = ((float) py + 0.5f) / kS;
            const float x = fx - mCx, y = fy - mCy, r = std::hypot (x, y);
            const float du = x * ca + y * sa, dn = x * sa - y * ca;

            float v = 0.0f, A = 0.73f * sampleMask (shadow, mW, mH, fx, fy);

            const float tLight = sat ((dn - 7.32f) * kS + 0.5f);
            const float tDark  = sat ((-7.41f - dn) / 5.41f);
            const float body = 49.0f + (85.0f - 49.0f) * tLight + (20.0f - 49.0f) * tDark;
            const float inside = sat ((R - r) * kS + 0.5f);
            v = v * (1 - inside) + body * inside;
            A = A * (1 - inside) + inside;

            const float pm = sat (1.0f - std::abs (dn + 0.3f) / 1.03f + 0.5f) * sat ((du - 6.1f) * kS + 0.5f) * sat ((22.88f - du) * kS + 0.5f);
            v = v * (1 - pm) + 197.0f * pm;
            A = A * (1 - pm) + pm;

            const auto g8 = (uint8) jlimit (0, 255, roundToInt (v));
            bd.setPixelColour (px, py, Colour (g8, g8, g8, (uint8) jlimit (0, 255, roundToInt (A * 255.0f))));
        }
    return im;
}

void RefLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int width, int height,
                                       float pos, float startAngle, float endAngle, Slider& s)
{
    const auto c = Rectangle<float> ((float) x, (float) y, (float) width, (float) height).getCentre();
    const float a = startAngle + pos * (endAngle - startAngle);
    const bool flat = (int) s.getProperties().getWithDefault ("flat", 0) != 0;

    g.setImageResamplingQuality (Graphics::highResamplingQuality);
    auto pt = [&] (float rad) { return Point<float> (c.x + std::sin (a) * rad, c.y - std::cos (a) * rad); };
    Path p;
    if (flat)
    {
        const float m = ui::midScale, h = (float) ui::flatHalf * m;
        g.drawImage (flatImg, Rectangle<float> (c.x - h, c.y - h, h * 2.0f, h * 2.0f), RectanglePlacement::stretchToFit);
        p.startNewSubPath (pt (11.0f * m));        // puntero: del centro al borde
        p.lineTo (pt (34.0f * m));
        g.setColour (Colour (0xffe4e4e4));
        g.strokePath (p, PathStrokeType (2.9f * m, PathStrokeType::mitered, PathStrokeType::butt));
    }
    else
    {
        const float h = (float) ui::ringHalf;
        g.drawImage (ringImg, Rectangle<float> (c.x - h, c.y - h, h * 2.0f, h * 2.0f), RectanglePlacement::stretchToFit);
        p.startNewSubPath (pt (100.2f));           // puntero: del borde de la tapa al borde del aro
        p.lineTo (pt (131.0f));
        g.setColour (Colour (0xffe8e8e8));
        g.strokePath (p, PathStrokeType (3.2f, PathStrokeType::mitered, PathStrokeType::butt));
    }
}

// Fader vertical de la referencia (Window): ranura negra, 5 pares de marcas y tirador con linea clara
void RefLookAndFeel::drawLinearSlider (Graphics& g, int x, int, int width, int,
                                       float sliderPos, float, float,
                                       Slider::SliderStyle, Slider& slider)
{
    const float cx = (float) x + (float) width * 0.5f;
    const float p0 = (float) slider.getPositionOfValue (slider.getMinimum()), p1 = (float) slider.getPositionOfValue (slider.getMaximum());
    const float top = jmin (p0, p1), bottom = jmax (p0, p1);

    // marcas cada 3 dB (+6, +3, 0, -3, -6, -9, -12), en la posicion real de cada valor
    g.setColour (Colour (0xffa6a6a6));
    for (float db : { 6.0f, 3.0f, 0.0f, -3.0f, -6.0f, -9.0f, -12.0f })
    {
        const float ty = (float) slider.getPositionOfValue ((double) db);
        g.fillRect (cx - 34.5f, ty - 1.0f, 14.0f, 2.0f);
        g.fillRect (cx + 20.5f, ty - 1.0f, 14.0f, 2.0f);
    }

    // ranura
    g.setColour (Colour (0xff000000));
    g.fillRoundedRectangle (cx - 2.3f, top - 1.0f, 4.6f, bottom - top + 2.0f, 2.3f);

    // tirador (copiado de la referencia, medido pixel a pixel): cuerpo plano 49 x 18.5 con bandas de gris,
    // linea clara en la marca central y dos sombras (una corta y nitida debajo, otra amplia abajo-derecha)
    const float lineC = sliderPos + 1.5f;
    const float k = 1.25f;   // agrandado respecto de la referencia
    const Rectangle<float> th (cx - 24.5f * k, lineC - 13.2f * k, 49.0f * k, 18.5f * k);
    {
        Path tp; tp.addRectangle (th);
        DropShadow (Colours::black.withAlpha (0.75f), 30, { 15, 17 }).drawForPath (g, tp);
        DropShadow (Colours::black.withAlpha (0.35f), 14, { 8, 8 }).drawForPath (g, tp);
        Path tight; tight.addRectangle (th.translated (0.0f, 7.5f * k).reduced (0.0f, 1.0f));
        DropShadow (Colours::black.withAlpha (0.5f), 2, { 0, 0 }).drawForPath (g, tight);
    }
    {
        ColourGradient cg (Colour (0xff3b3b3b), 0.0f, th.getY(), Colour (0xff313131), 0.0f, th.getBottom(), false);
        auto at = [&] (float row, uint8 v) { cg.addColour (row / 18.5f, Colour (v, v, v)); };
        at (3.5f, 0x3d); at (5.0f, 0x47); at (7.5f, 0x47); at (9.0f, 0x3c); at (10.0f, 0x2e); at (11.5f, 0x2f);
        at (12.0f, 0x3d); at (14.5f, 0x32);
        g.setGradientFill (cg);
        g.fillRect (th);
    }
    g.setColour (Colour (0xffdbdbdb));
    g.fillRect (th.getX(), lineC - 1.5f, th.getWidth(), 3.0f);
}

void RefLookAndFeel::drawCapText (Graphics& g, const String& s, float x, float capCentreY, float em, bool medium,
                                  Colour col, Justification just) const
{
    g.setColour (col);
    g.setFont (medium ? mediumFont (em) : regularFont (em));
    g.drawSingleLineText (s, roundToInt (x), roundToInt (capCentreY + 0.3635f * em), just);
}

//==============================================================================
static void drawFlatSquare (Graphics& g, Rectangle<float> r, bool on)
{
    Path sq;
    sq.addRectangle (r.expanded (1.0f));
    DropShadow (Colours::black.withAlpha (0.62f), 9, { 3, 5 }).drawForPath (g, sq);

    if (on)
    {
        g.setColour (Colour (0xff125755));                       // contorno verde oscuro
        g.fillRect (r.expanded (1.0f));
        g.setColour (Colour (0xff3cf4ef));                       // marco cyan
        g.fillRect (r);
        g.setColour (Colour (0xff59ffff).withAlpha (0.55f));     // luz fina en el borde
        g.drawRect (r.reduced (0.5f), 1.0f);
        auto ring = r.reduced (r.getWidth() * 0.145f);
        g.setColour (Colour (0xff2fe6da));                       // anillo fino
        g.drawRect (ring, 1.6f);
        g.setColour (Colour (0xffb0fffa));                       // centro palido
        g.fillRect (r.reduced (r.getWidth() * 0.225f));
    }
    else
    {
        g.setColour (Colour (0xff0f0f0f));                       // contorno negro
        g.fillRect (r.expanded (1.0f));
        g.setColour (Colour (0xff555555));                       // marco gris
        g.fillRect (r);
        g.setColour (Colour (0xff787878));                       // luz en el borde izquierdo
        g.fillRect (r.getX(), r.getY(), 1.5f, r.getHeight());
        auto inner = r.reduced (r.getWidth() * 0.207f);
        g.setColour (Colour (0xff6a6a6a));                       // reborde claro del hueco
        g.drawRect (inner.expanded (1.5f), 1.5f);
        g.setColour (Colour (0xff444444));                       // hueco plano
        g.fillRect (inner);
    }
}

PushButton::PushButton (RangedAudioParameter& p, const String& t, RefLookAndFeel& l)
    : title (t), laf (l), attachment (p, [this] (float v) { on = v > 0.5f; repaint(); })
{
    attachment.sendInitialUpdate();
    setMouseCursor (MouseCursor::PointingHandCursor);
}

void PushButton::mouseDown (const MouseEvent&)
{
    attachment.setValueAsCompleteGesture (on ? 0.0f : 1.0f);
}

void PushButton::paint (Graphics& g)
{
    const float cx = (float) getWidth() * 0.5f;
    laf.drawCapText (g, title, cx, 10.0f, ui::emLabel, true, ui::label, Justification::horizontallyCentred);
    drawFlatSquare (g, Rectangle<float> (cx - 23.0f, 35.0f, 46.0f, 46.0f), on);
    laf.drawCapText (g, on ? "On" : "Off", cx, 105.0f, ui::emValue, false, ui::caption, Justification::horizontallyCentred);
}

//==============================================================================
MiniToggle::MiniToggle (RangedAudioParameter& p, const String& t, const String& top, const String& bottom, RefLookAndFeel& l)
    : title (t), topText (top), bottomText (bottom), laf (l),
      attachment (p, [this] (float v) { bottomOn = v > 0.5f; repaint(); })
{
    attachment.sendInitialUpdate();
    setMouseCursor (MouseCursor::PointingHandCursor);
}

void MiniToggle::mouseDown (const MouseEvent&)
{
    attachment.setValueAsCompleteGesture (bottomOn ? 0.0f : 1.0f);
}

void MiniToggle::paint (Graphics& g)
{
    // el disco mide lo mismo que la perilla de Polarity; todos los textos usan la misma tipografia
    const float R = 36.26f * ui::midScale, k = R / 23.11f, m = R / 30.74f;
    const float cx = 110.0f, cy = 40.0f + ui::modeTitleToDisc;

    laf.drawCapText (g, title, cx, 40.0f, ui::emLabel, true, ui::label, Justification::horizontallyCentred);

    // lineas guia (antes del disco: su sombra las oscurece, como en la referencia)
    auto leader = [&] (float dir, const String& s)
    {
        const float y1 = cy + dir * 34.0f * m;
        const float x0 = cx + 22.5f * m, y0 = cy + dir * 22.5f * m;     // borde del disco a 45 grados
        Path p; p.startNewSubPath (x0, y0); p.lineTo (x0 + 11.0f * m, y1); p.lineTo (x0 + 11.0f * m + 13.0f, y1);
        g.setColour (Colour (0xffb4b4b4));
        g.strokePath (p, PathStrokeType (1.5f));
        laf.drawCapText (g, s, x0 + 11.0f * m + 20.0f, y1, ui::emNumber, false, ui::caption, Justification::left);
    };
    leader (-1.0f, topText);
    leader (+1.0f, bottomText);

    g.setImageResamplingQuality (Graphics::highResamplingQuality);
    g.drawImage (bottomOn ? laf.modeWholeImg : laf.modeHalfImg,
                 Rectangle<float> (cx - 50.0f * k, cy - 50.0f * k, 100.0f * k, 100.0f * k), RectanglePlacement::stretchToFit);
}

//==============================================================================
void GrMeter::paint (Graphics& g)
{
    const float cx = (float) getWidth() * 0.5f - 18.0f;
    const float top = 12.0f, pitch = (float) (ui::meterBottomY - ui::meterTopY) / 24.0f;
    auto thresholdDb = [] (int i) { return 6.0f - 0.75f * (float) i; };      // segmento 24 = -inf
    int peakIdx = -1;
    if (peak > -80.0f)
        peakIdx = peak >= 6.0f ? 0 : jlimit (0, 23, (int) std::ceil ((6.0f - peak) / 0.75f));

    static const char* names[7] = { "+6", "+3", "0", "-3", "-6", "-9", "-inf" };

    for (int i = 0; i < 25; ++i)
    {
        const float cy = top + pitch * (float) i;
        const Rectangle<float> r (cx - 15.5f, cy - 5.5f, 31.0f, 11.0f);
        const bool lit = (i < 24 ? level >= thresholdDb (i) : level > -80.0f) || i == peakIdx;

        g.setColour (Colour (0xff101010));
        g.fillRect (r.expanded (1.0f));
        if (lit)
        {
            g.setColour (i == 24 ? Colour (0xffd5fffc) : Colour (0xfff6f6f6));
            g.fillRect (r);
        }
        else
        {
            g.setGradientFill (ColourGradient (Colour (0xff3a3a3a), 0.0f, r.getY(), Colour (0xff424242), 0.0f, r.getBottom(), false));
            g.fillRect (r);
        }
        if (i % 4 == 0)
            laf.drawCapText (g, names[i / 4], cx + 15.5f + 18.5f, cy, ui::emValue, false, ui::value, Justification::horizontallyCentred);
    }
}

//==============================================================================
//==============================================================================
PhantomAsymEditor::PhantomAsymEditor (PhantomAsymProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      modeToggle   (*p.apvts.getParameter ("mode"),   "Mode",         "Half", "Whole", laf),
      bypassButton (*p.apvts.getParameter ("bypass"), "Depth Bypass", laf),
      listenButton (*p.apvts.getParameter ("listen"), "Key Listen",   laf),
      meter (laf)
{
    setLookAndFeel (&laf);

    auto setup = [this] (KnobSlider& k, int textBoxHeight)
    {
        k.setTextBoxStyle (Slider::TextBoxBelow, false, 150, textBoxHeight);
        k.setTextBoxIsEditable (true);
        k.setColour (Slider::textBoxOutlineColourId,    Colours::transparentBlack);
        k.setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
        k.setColour (Slider::textBoxTextColourId,       ui::value);
        k.setDoubleClickReturnValue (false, 0.0);
        k.setPaintingIsUnclipped (true);          // las sombras salen del rectangulo de la perilla
        addAndMakeVisible (k);
    };
    for (auto* k : { &depthKnob, &polarityKnob })
    {
        k->setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
        k->setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
    }
    polarityKnob.getProperties().set ("flat", 1);
    outputKnob.setSliderStyle (Slider::LinearVertical);
    setup (depthKnob, 18);
    setup (polarityKnob, ui::polTextH);
    setup (outputKnob, ui::faderTextH);

    depthKnob.hitCy = ui::depthAreaH * 0.5f;    depthKnob.hitR = 138.0f;
    polarityKnob.hitCy = ui::polAreaH * 0.5f;   polarityKnob.hitR = 40.0f * ui::midScale;
    outputKnob.hitCy = -1.0f;

    depthAtt    = std::make_unique<APVTS::SliderAttachment> (proc.apvts, "depthdb",  depthKnob);
    polarityAtt = std::make_unique<APVTS::SliderAttachment> (proc.apvts, "polarity", polarityKnob);
    outputAtt   = std::make_unique<APVTS::SliderAttachment> (proc.apvts, "output",   outputKnob);

    for (auto* k : { &depthKnob, &polarityKnob, &outputKnob })
        k->setDoubleClickReturnValue (false, 0.0);   // el attachment lo reactiva: el doble clic es para escribir el valor

    // unidad "dB" junto al valor (despues de crear los attachments, que fijan el texto)
    for (auto [knob, id] : { std::pair<KnobSlider*, const char*> { &depthKnob, "depthdb" }, { &outputKnob, "output" } })
    {
        auto* prm = proc.apvts.getParameter (id);
        knob->textFromValueFunction = [prm] (double v) { return prm->getText (prm->convertTo0to1 ((float) v), 0) + " dB"; };
        knob->updateText();
    }

    addAndMakeVisible (modeToggle);
    addAndMakeVisible (bypassButton);
    addAndMakeVisible (listenButton);
    addAndMakeVisible (meter);
    startTimerHz (30);

    setSize (ui::width, ui::height);
    setScaleFactor (ui::windowScale);
}

PhantomAsymEditor::~PhantomAsymEditor()
{
    setLookAndFeel (nullptr);
}

void PhantomAsymEditor::paint (Graphics& g)
{
    g.fillAll (ui::bg);

    // marco de la ventana (valores medidos en la referencia)
    const float w = (float) getWidth(), h = (float) getHeight();
    g.setColour (Colour (0xff767676)); g.fillRect (0.0f, 0.0f, w, 1.0f);
    g.setColour (Colour (0xff949494)); g.fillRect (0.0f, 0.0f, 1.0f, h); g.fillRect (w - 1.0f, 0.0f, 1.0f, h);

    // separador vertical
    const float sx = (float) ui::sepX, y0 = (float) ui::sepTop, y1 = (float) ui::sepBottom;
    g.setColour (Colour (0xff252525)); g.fillRect (sx - 1.0f, y0, 1.0f, y1 - y0);
    g.setColour (Colour (0xff787878)); g.fillRect (sx,        y0, 1.0f, y1 - y0);
    g.setColour (Colour (0xff909090)); g.fillRect (sx + 1.0f, y0, 1.0f, y1 - y0);

    // nombres
    laf.drawCapText (g, "Depth",    (float) ui::depthX, (float) ui::depthLabelY, ui::emLabel,                  true, ui::label, Justification::horizontallyCentred);
    laf.drawCapText (g, "Polarity", (float) ui::polX,   (float) ui::polLabelY,   ui::emLabel,                  true, ui::label, Justification::horizontallyCentred);
    laf.drawCapText (g, "SCD-1", 32.0f, 38.0f, ui::emTitle, true, ui::label, Justification::left);   // nombre del plugin
    laf.drawCapText (g, "Output",   (float) ui::faderX, (float) ui::topLabelY,   ui::emLabel,                  true, ui::label, Justification::horizontallyCentred);
}

void PhantomAsymEditor::timerCallback()
{
    if (preview)
        return;

    const float dt = 1.0f / 30.0f;
    const float v = proc.outMeterDb.load();
    meterLevel = v >= meterLevel ? v : jmax (v, meterLevel - 24.0f * dt);
    const double now = Time::getMillisecondCounterHiRes() * 0.001;
    if (meterLevel >= meterPeak) { meterPeak = meterLevel; peakHoldUntil = now + 1.2; }
    else if (now > peakHoldUntil) meterPeak = jmax (meterLevel, meterPeak - 12.0f * dt);
    meter.setLevels (meterLevel, meterPeak);
}

void PhantomAsymEditor::resized()
{
    depthKnob.setBounds    (ui::depthX - 150, ui::depthY - (int) (ui::depthAreaH / 2), 300, (int) ui::depthAreaH + 18);
    polarityKnob.setBounds (ui::polX - 90,    ui::polY - (int) (ui::polAreaH / 2),     180, (int) ui::polAreaH + ui::polTextH);
    outputKnob.setBounds   (ui::faderX - 60,  ui::faderTop, 120, ui::faderLen + ui::faderTextH);

    modeToggle.setBounds   (ui::modeX - 110, ui::modeTitleY - 40, 300, 300);
    bypassButton.setBounds (ui::btnX[1] - 55, ui::btnLabelY - 10, 110, 115);
    listenButton.setBounds (ui::btnX[0] - 55, ui::btnLabelY - 10, 110, 115);
    meter.setBounds        (ui::meterX - 42, ui::meterTopY - 12, 120, ui::meterBottomY - ui::meterTopY + 24);
}
