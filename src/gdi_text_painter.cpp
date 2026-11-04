#include "gdi_text_painter.h"

GdiTextPainter::~GdiTextPainter() {
    if (font_) DeleteObject(font_);
}

void GdiTextPainter::SetStyle(const TextStyle& style) {
    style_ = style;
    fontValid_ = false;
}

void GdiTextPainter::EnsureFont() {
    if (fontValid_ && font_) return;

    if (font_) { DeleteObject(font_); font_ = nullptr; }

    font_ = CreateFontW(
        -style_.fontSize, 0, 0, 0, style_.fontWeight,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
        style_.fontFace);

    fontValid_ = font_ != nullptr;
}

SIZE GdiTextPainter::Measure(HDC dc, const wchar_t* text) const {
    SIZE size = { 0, 0 };
    if (!dc || !text) return size;

    const_cast<GdiTextPainter*>(this)->EnsureFont();
    if (!font_ || !text[0]) return size;

    HGDIOBJ old = SelectObject(dc, font_);
    GetTextExtentPoint32W(dc, text, lstrlenW(text), &size);
    SelectObject(dc, old);
    return size;
}

SIZE GdiTextPainter::MeasureHint(HDC dc, const wchar_t* text, int pixelHeight) const {
    SIZE size = { 0, 0 };
    if (!dc || !text || !text[0] || pixelHeight <= 0) return size;

    HFONT font = CreateFontW(
        -pixelHeight, 0, 0, 0, FW_NORMAL,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
        style_.fontFace);
    if (!font) return size;

    HGDIOBJ old = SelectObject(dc, font);
    GetTextExtentPoint32W(dc, text, lstrlenW(text), &size);
    SelectObject(dc, old);
    DeleteObject(font);
    return size;
}

void GdiTextPainter::DrawHint(HDC dc, const wchar_t* text, int x, int y,
                              int pixelHeight, COLORREF color) const {
    if (!dc || !text || !text[0] || pixelHeight <= 0) return;

    HFONT font = CreateFontW(
        -pixelHeight, 0, 0, 0, FW_NORMAL,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
        style_.fontFace);
    if (!font) return;

    HGDIOBJ old = SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    TextOutW(dc, x, y, text, lstrlenW(text));
    SelectObject(dc, old);
    DeleteObject(font);
}

void GdiTextPainter::Draw(HDC dc, const wchar_t* text, int x, int y) const {
    if (!dc || !text || !text[0]) return;

    const_cast<GdiTextPainter*>(this)->EnsureFont();
    if (!font_) return;

    HGDIOBJ old = SelectObject(dc, font_);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, style_.textColor);
    TextOutW(dc, x, y, text, lstrlenW(text));
    SelectObject(dc, old);
}