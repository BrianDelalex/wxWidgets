///////////////////////////////////////////////////////////////////////////////

// Name:        src/aui/auibar.cpp
// Purpose:     wxaui: wx advanced user interface - docking window manager
// Author:      Benjamin I. Williams
// Created:     2005-05-17
// Copyright:   (C) Copyright 2005-2006, Kirix Corporation, All Rights Reserved
// Licence:     wxWindows Library Licence, Version 3.1
///////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#include "wx/wxprec.h"


#if wxUSE_AUI

#include "wx/statline.h"
#include "wx/dcbuffer.h"
#include "wx/frame.h"
#include "wx/sizer.h"
#include "wx/image.h"
#include "wx/settings.h"
#include "wx/menu.h"

#include "wx/aui/auibar.h"
#include "wx/aui/framemanager.h"

#ifndef WX_PRECOMP
    #include "wx/dcclient.h"
#endif

#ifdef __WXMAC__
#include "wx/osx/private.h"
#endif

#include "wx/private/aui.h"

wxDEFINE_EVENT( wxEVT_AUITOOLBAR_TOOL_DROPDOWN, wxAuiToolBarEvent );
wxDEFINE_EVENT( wxEVT_AUITOOLBAR_OVERFLOW_CLICK, wxAuiToolBarEvent );
wxDEFINE_EVENT( wxEVT_AUITOOLBAR_RIGHT_CLICK, wxAuiToolBarEvent );
wxDEFINE_EVENT( wxEVT_AUITOOLBAR_MIDDLE_CLICK, wxAuiToolBarEvent );
wxDEFINE_EVENT( wxEVT_AUITOOLBAR_BEGIN_DRAG, wxAuiToolBarEvent );


wxIMPLEMENT_CLASS(wxAuiToolBar, wxControl);
wxIMPLEMENT_DYNAMIC_CLASS(wxAuiToolBarEvent, wxEvent);


// missing wxITEM_* items
enum
{
    wxITEM_CONTROL = wxITEM_MAX,
    wxITEM_LABEL,
    wxITEM_SPACER
};


static wxColor GetBaseColor()
{

    wxColor baseColour = wxSystemSettings::GetColour(wxSYS_COLOUR_3DFACE);

    // the baseColour is too pale to use as our base colour,
    // so darken it a bit --
    if ((255-baseColour.Red()) +
        (255-baseColour.Green()) +
        (255-baseColour.Blue()) < 60)
    {
        baseColour = baseColour.ChangeLightness(92);
    }

    return baseColour;
}

static bool IsThemeDark()
{
    return wxSystemSettings::GetAppearance().IsDark();
}



class ToolbarCommandCapture : public wxEvtHandler
{
public:

    ToolbarCommandCapture() { m_lastId = 0; }
    int GetCommandId() const { return m_lastId; }

    bool ProcessEvent(wxEvent& evt) override
    {
        if (evt.GetEventType() == wxEVT_MENU)
        {
            m_lastId = evt.GetId();
            return true;
        }

        if (GetNextHandler())
            return GetNextHandler()->ProcessEvent(evt);

        return false;
    }

private:
    int m_lastId;
};

wxBitmap wxAuiToolBarItem::GetCurrentBitmapFor(wxWindow* wnd) const
{
    // We suppose that we don't have disabled bitmap if we don't have the
    // normal one either.
    if ( !m_bitmap.IsOk() )
        return wxNullBitmap;

    if ( m_state & wxAUI_BUTTON_STATE_DISABLED )
    {
        if ( m_disabledBitmap.IsOk() )
            return m_disabledBitmap.GetBitmapFor(wnd);

        return m_bitmap.GetBitmapFor(wnd).ConvertToDisabled();
    }

    return m_bitmap.GetBitmapFor(wnd);
}

int
wxAuiToolBarArt::GetElementSizeForWindow(int elementId, const wxWindow* window)
{
    return wxWindow::FromDIP(GetElementSize(elementId), window);
}

wxAuiGenericToolBarArt::wxAuiGenericToolBarArt()
{
    UpdateColoursFromSystem();

    m_flags = 0;
    m_textOrientation = wxAUI_TBTOOL_TEXT_BOTTOM;

    // Note that these values are intentionally in DIPs.
    m_separatorSize =  7;
    m_gripperSize   =  7;
    m_overflowSize  = 16;
    m_dropdownSize  = 10;


    m_font = *wxNORMAL_FONT;
}

wxAuiGenericToolBarArt::~wxAuiGenericToolBarArt()
{
    m_font = *wxNORMAL_FONT;
}
wxAuiToolBarArt* wxAuiGenericToolBarArt::Clone()
{
    return static_cast<wxAuiToolBarArt*>(new wxAuiGenericToolBarArt);
}

void wxAuiGenericToolBarArt::UpdateColoursFromSystem()
{
    m_baseColour = GetBaseColor();
    m_highlightColour = wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHT);
    wxColor darker3Colour = m_baseColour.ChangeLightness(60);
    wxColor darker5Colour = m_baseColour.ChangeLightness(40);

    int pen_width = wxWindow::FromDIP(1, nullptr);
    m_gripperPen1 = wxPen(darker5Colour, pen_width);
    m_gripperPen2 = wxPen(darker3Colour, pen_width);
    m_gripperPen3 = wxPen(*wxStockGDI::GetColour(wxStockGDI::COLOUR_WHITE), pen_width);

    // Note: update the bitmaps here as they depend on the system colours too.

#ifdef wxHAS_SVG
    static const char* const buttonDropdownBitmapData = R"svg(
<svg version="1.0" xmlns="http://www.w3.org/2000/svg" width="5" height="3">
    <polygon points="0, 0 5 0 2.5, 2" stroke="currentColor" fill="currentColor" stroke-width="0"/>
</svg>
)svg";

    static const char* const overflowBitmapData = R"svg(
<svg version="1.0" xmlns="http://www.w3.org/2000/svg" width="7" height="6">
    <rect x="0" y="0" width="7" height="1" stroke="currentColor" fill="currentColor" stroke-width="0"/>
    <polygon points="0, 2 7 2 3.5, 6" stroke="currentColor" fill="currentColor" stroke-width="0"/>
</svg>
)svg";
#else // !wxHAS_SVG
    static const unsigned char buttonDropdownBitmapData[] = { 0xe0, 0xf1, 0xfb };
    static const unsigned char overflowBitmapData[] = { 0x80, 0xff, 0x80, 0xc1, 0xe3, 0xf7 };
#endif // wxHAS_SVG/!wxHAS_SVG

    m_buttonDropDownBmp = wxAuiCreateBitmap(buttonDropdownBitmapData, 5, 3,
                                              wxSystemSettings::GetColour(wxSYS_COLOUR_BTNTEXT));
    m_disabledButtonDropDownBmp = wxAuiCreateBitmap(
                                                buttonDropdownBitmapData, 5, 3,
                                                wxColor(128,128,128));
    m_overflowBmp = wxAuiCreateBitmap(overflowBitmapData, 7, 6,
                                        wxSystemSettings::GetColour(wxSYS_COLOUR_BTNTEXT));
    m_disabledOverflowBmp = wxAuiCreateBitmap(overflowBitmapData, 7, 6, wxColor(128,128,128));
}

void wxAuiGenericToolBarArt::SetFlags(unsigned int flags)
{
    m_flags = flags;
}

void wxAuiGenericToolBarArt::SetFont(const wxFont& font)
{
    m_font = font;
}

void wxAuiGenericToolBarArt::SetTextOrientation(int orientation)
{
    m_textOrientation = orientation;
}

unsigned int wxAuiGenericToolBarArt::GetFlags()
{
    return m_flags;
}

wxFont wxAuiGenericToolBarArt::GetFont()
{
    return m_font;
}

int wxAuiGenericToolBarArt::GetTextOrientation()
{
    return m_textOrientation;
}

void wxAuiGenericToolBarArt::DrawBackground(
                                    wxDC& dc,
                                    wxWindow* WXUNUSED(wnd),
                                    const wxRect& _rect)
{
    wxRect rect = _rect;
    rect.height++;

    int startLightness = 150;
    int endLightness = 90;

    if ((m_baseColour.Red() < 75)
        && (m_baseColour.Green() < 75)
        && (m_baseColour.Blue() < 75))
    {
        //dark mode, we cannot go very light
        startLightness = 110;
        endLightness = 90;
    }
    wxColour startColour = m_baseColour.ChangeLightness(startLightness);
    wxColour endColour = m_baseColour.ChangeLightness(endLightness);

    dc.GradientFillLinear(rect, startColour, endColour, wxSOUTH);
}

void wxAuiGenericToolBarArt::DrawPlainBackground(wxDC& dc,
                                                   wxWindow* WXUNUSED(wnd),
                                                   const wxRect& rect)
{
    dc.SetBrush(wxSystemSettings::GetColour(wxSYS_COLOUR_3DFACE));
    dc.SetPen(*wxTRANSPARENT_PEN);

    dc.DrawRectangle(rect);
}

void wxAuiGenericToolBarArt::DrawLabel(
                                    wxDC& dc,
                                    wxWindow* WXUNUSED(wnd),
                                    const wxAuiToolBarItem& item,
                                    const wxRect& rect)
{
    dc.SetFont(m_font);
    dc.SetTextForeground(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNTEXT));

    // we only care about the text height here since the text
    // will get cropped based on the width of the item
    int textWidth = 0, textHeight = 0;
    dc.GetTextExtent(wxT("ABCDHgj"), &textWidth, &textHeight);

    // set the clipping region
    wxRect clipRect = rect;
    clipRect.width -= 1;
    wxDCClipper clipper(dc, clipRect);

    int textX, textY;
    textX = rect.x + 1;
    textY = rect.y + (rect.height-textHeight)/2;
    dc.DrawText(item.GetLabel(), textX, textY);
}


void wxAuiGenericToolBarArt::DrawButton(
                                    wxDC& dc,
                                    wxWindow* wnd,
                                    const wxAuiToolBarItem& item,
                                    const wxRect& rect)
{
    int textWidth = 0, textHeight = 0;

    if (m_flags & wxAUI_TB_TEXT)
    {
        dc.SetFont(m_font);

        int tx, ty;

        dc.GetTextExtent(wxT("ABCDHgj"), &tx, &textHeight);
        textWidth = 0;
        dc.GetTextExtent(item.GetLabel(), &textWidth, &ty);
    }

    int bmpX = 0, bmpY = 0;
    int textX = 0, textY = 0;

    const wxBitmap& bmp = item.GetCurrentBitmapFor(wnd);
    const wxSize bmpSize = bmp.IsOk() ? bmp.GetLogicalSize() : wxSize(0, 0);

    if (m_textOrientation == wxAUI_TBTOOL_TEXT_BOTTOM)
    {
        bmpX = rect.x +
                (rect.width/2) -
                (bmpSize.x/2);

        bmpY = rect.y +
                ((rect.height-textHeight)/2) -
                (bmpSize.y/2);

        textX = rect.x + (rect.width/2) - (textWidth/2) + 1;
        textY = rect.y + rect.height - textHeight - 1;
    }
    else if (m_textOrientation == wxAUI_TBTOOL_TEXT_RIGHT)
    {
        bmpX = rect.x + wnd->FromDIP(3);

        bmpY = rect.y +
                (rect.height/2) -
                (bmpSize.y/2);

        textX = bmpX + wnd->FromDIP(3) + bmpSize.x;
        textY = rect.y +
                 (rect.height/2) -
                 (textHeight/2);
    }


    if (!(item.GetState() & wxAUI_BUTTON_STATE_DISABLED))
    {
        if (item.GetState() & wxAUI_BUTTON_STATE_PRESSED)
        {
            dc.SetPen(wxPen(m_highlightColour));
            dc.SetBrush(wxBrush(m_highlightColour.ChangeLightness(IsThemeDark() ? 20 : 150)));
            dc.DrawRectangle(rect);
        }
        else if ((item.GetState() & wxAUI_BUTTON_STATE_HOVER) || item.IsSticky())
        {
            dc.SetPen(wxPen(m_highlightColour));
            dc.SetBrush(wxBrush(m_highlightColour.ChangeLightness(IsThemeDark() ? 40 : 170)));

            // draw an even lighter background for checked item hovers (since
            // the hover background is the same color as the check background)
            if (item.GetState() & wxAUI_BUTTON_STATE_CHECKED)
                dc.SetBrush(wxBrush(m_highlightColour.ChangeLightness(IsThemeDark() ? 50 : 180)));

            dc.DrawRectangle(rect);
        }
        else if (item.GetState() & wxAUI_BUTTON_STATE_CHECKED)
        {
            // it's important to put this code in an else statement after the
            // hover, otherwise hovers won't draw properly for checked items
            dc.SetPen(wxPen(m_highlightColour));
            dc.SetBrush(wxBrush(m_highlightColour.ChangeLightness(IsThemeDark() ? 40 : 170)));
            dc.DrawRectangle(rect);
        }
    }

    if ( bmp.IsOk() )
        dc.DrawBitmap(bmp, bmpX, bmpY, true);

    // set the item's text color based on if it is disabled
    dc.SetTextForeground(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNTEXT));
    if (item.GetState() & wxAUI_BUTTON_STATE_DISABLED)
    {
        dc.SetTextForeground(wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT));
    }

    if ( (m_flags & wxAUI_TB_TEXT) && !item.GetLabel().empty() )
    {
        dc.DrawText(item.GetLabel(), textX, textY);
    }
}


void wxAuiGenericToolBarArt::DrawDropDownButton(
                                    wxDC& dc,
                                    wxWindow* wnd,
                                    const wxAuiToolBarItem& item,
                                    const wxRect& rect)
{
    int dropdownWidth = GetElementSizeForWindow(wxAUI_TBART_DROPDOWN_SIZE, wnd);
    int textWidth = 0, textHeight = 0, textX = 0, textY = 0;
    int bmpX = 0, bmpY = 0, dropBmpX = 0, dropBmpY = 0;

    wxRect buttonRect = wxRect(rect.x,
                                rect.y,
                                rect.width-dropdownWidth,
                                rect.height);
    wxRect dropDownRect = wxRect(rect.x+rect.width-dropdownWidth-1,
                                  rect.y,
                                  dropdownWidth+1,
                                  rect.height);

    if (m_flags & wxAUI_TB_TEXT)
    {
        dc.SetFont(m_font);

        int tx, ty;
        if (m_flags & wxAUI_TB_TEXT)
        {
            dc.GetTextExtent(wxT("ABCDHgj"), &tx, &textHeight);
            textWidth = 0;
        }

        dc.GetTextExtent(item.GetLabel(), &textWidth, &ty);
    }



    const wxSize sizeDropDown = m_buttonDropDownBmp.GetPreferredLogicalSizeFor(wnd);
    dropBmpX = dropDownRect.x +
                (dropDownRect.width/2) -
                (sizeDropDown.x/2);
    dropBmpY = dropDownRect.y +
                (dropDownRect.height/2) -
                (sizeDropDown.y/2);


    const wxBitmap& bmp = item.GetCurrentBitmapFor(wnd);

    if (m_textOrientation == wxAUI_TBTOOL_TEXT_BOTTOM)
    {
        bmpX = buttonRect.x +
                (buttonRect.width/2) -
                (bmp.GetLogicalWidth()/2);
        bmpY = buttonRect.y +
                ((buttonRect.height-textHeight)/2) -
                (bmp.GetLogicalHeight()/2);

        textX = rect.x + (rect.width/2) - (textWidth/2) + 1;
        textY = rect.y + rect.height - textHeight - 1;
    }
    else if (m_textOrientation == wxAUI_TBTOOL_TEXT_RIGHT)
    {
        bmpX = rect.x + wnd->FromDIP(3);

        bmpY = rect.y +
                (rect.height/2) -
                (bmp.GetLogicalHeight()/2);

        textX = bmpX + wnd->FromDIP(3) + bmp.GetLogicalWidth();
        textY = rect.y +
                 (rect.height/2) -
                 (textHeight/2);
    }


    if (item.GetState() & wxAUI_BUTTON_STATE_PRESSED)
    {
        dc.SetPen(wxPen(m_highlightColour));
        dc.SetBrush(wxBrush(m_highlightColour.ChangeLightness(IsThemeDark() ? 10 : 140)));
        dc.DrawRectangle(buttonRect);

        dc.SetBrush(wxBrush(m_highlightColour.ChangeLightness(IsThemeDark() ? 40 : 170)));
        dc.DrawRectangle(dropDownRect);
    }
    else if (item.GetState() & wxAUI_BUTTON_STATE_HOVER ||
             item.IsSticky())
    {
        dc.SetPen(wxPen(m_highlightColour));
        dc.SetBrush(wxBrush(m_highlightColour.ChangeLightness(IsThemeDark() ? 40 : 170)));
        dc.DrawRectangle(buttonRect);
        dc.DrawRectangle(dropDownRect);
    }
    else if (item.GetState() & wxAUI_BUTTON_STATE_CHECKED)
    {
        // Notice that this branch must come after the hover one to ensure the
        // correct appearance when the mouse hovers over a checked item.m_
        dc.SetPen(wxPen(m_highlightColour));
        dc.SetBrush(wxBrush(m_highlightColour.ChangeLightness(IsThemeDark() ? 40 : 170)));
        dc.DrawRectangle(buttonRect);
        dc.DrawRectangle(dropDownRect);
    }

    if (!bmp.IsOk())
        return;

    wxBitmapBundle dropbmp;
    if (item.GetState() & wxAUI_BUTTON_STATE_DISABLED)
    {
        dropbmp = m_disabledButtonDropDownBmp;
    }
    else
    {
        dropbmp = m_buttonDropDownBmp;
    }

    dc.DrawBitmap(bmp, bmpX, bmpY, true);
    dc.DrawBitmap(dropbmp.GetBitmapFor(wnd), dropBmpX, dropBmpY, true);

    // set the item's text color based on if it is disabled
    dc.SetTextForeground(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNTEXT));
    if (item.GetState() & wxAUI_BUTTON_STATE_DISABLED)
    {
        dc.SetTextForeground(wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT));
    }

    if ( (m_flags & wxAUI_TB_TEXT) && !item.GetLabel().empty() )
    {
        dc.DrawText(item.GetLabel(), textX, textY);
    }
}

void wxAuiGenericToolBarArt::DrawControlLabel(
                                    wxDC& dc,
                                    wxWindow* WXUNUSED(wnd),
                                    const wxAuiToolBarItem& item,
                                    const wxRect& rect)
{
    if (!(m_flags & wxAUI_TB_TEXT))
        return;

    if (m_textOrientation != wxAUI_TBTOOL_TEXT_BOTTOM)
        return;

    int textX = 0, textY = 0;
    int textWidth = 0, textHeight = 0;

    dc.SetFont(m_font);

    int tx, ty;
    if (m_flags & wxAUI_TB_TEXT)
    {
        dc.GetTextExtent(wxT("ABCDHgj"), &tx, &textHeight);
        textWidth = 0;
    }

    dc.GetTextExtent(item.GetLabel(), &textWidth, &ty);

    // don't draw the label if it is wider than the item width
    if (textWidth > rect.width)
        return;

    // set the label's text color
    dc.SetTextForeground(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNTEXT));

    textX = rect.x + (rect.width/2) - (textWidth/2) + 1;
    textY = rect.y + rect.height - textHeight - 1;

    if ( (m_flags & wxAUI_TB_TEXT) && !item.GetLabel().empty() )
    {
        dc.DrawText(item.GetLabel(), textX, textY);
    }
}

wxSize wxAuiGenericToolBarArt::GetLabelSize(
                                        wxReadOnlyDC& dc,
                                        wxWindow* WXUNUSED(wnd),
                                        const wxAuiToolBarItem& item)
{
    dc.SetFont(m_font);

    // get label's height
    int width = 0, height = 0;
    dc.GetTextExtent(wxT("ABCDHgj"), &width, &height);

    // get item's width
    width = item.GetMinSize().GetWidth();

    if (width == -1)
    {
        // no width specified, measure the text ourselves
        width = dc.GetTextExtent(item.GetLabel()).GetX();
    }

    return wxSize(width, height);
}

wxSize wxAuiGenericToolBarArt::GetToolSize(
                                        wxReadOnlyDC& dc,
                                        wxWindow* wnd,
                                        const wxAuiToolBarItem& item)
{
    const wxBitmap& bmp = item.GetBitmapBundle().GetBitmapFor(wnd);
    if (!bmp.IsOk() && !(m_flags & wxAUI_TB_TEXT))
        return wnd->FromDIP(wxSize(16,16));

    int width = bmp.IsOk() ? bmp.GetLogicalWidth() : 0;
    int height = bmp.IsOk() ? bmp.GetLogicalHeight() : 0;

    if (m_flags & wxAUI_TB_TEXT)
    {
        dc.SetFont(m_font);
        int tx, ty;

        if (m_textOrientation == wxAUI_TBTOOL_TEXT_BOTTOM)
        {
            dc.GetTextExtent(wxT("ABCDHgj"), &tx, &ty);
            height += ty;

            if ( !item.GetLabel().empty() )
            {
                dc.GetTextExtent(item.GetLabel(), &tx, &ty);
                width = wxMax(width, tx+wnd->FromDIP(6));
            }
        }
        else if ( m_textOrientation == wxAUI_TBTOOL_TEXT_RIGHT &&
                  !item.GetLabel().empty() )
        {
            width += wnd->FromDIP(3); // space between left border and bitmap
            width += wnd->FromDIP(3); // space between bitmap and text

            if ( !item.GetLabel().empty() )
            {
                dc.GetTextExtent(item.GetLabel(), &tx, &ty);
                width += tx;
                height = wxMax(height, ty);
            }
        }
    }

    // if the tool has a dropdown button, add it to the width
    // and add some extra space in front of the drop down button
    if (item.HasDropDown())
    {
        int dropdownWidth = GetElementSizeForWindow(wxAUI_TBART_DROPDOWN_SIZE, wnd);
        width += dropdownWidth + wnd->FromDIP(4);
    }

    return wxSize(width, height);
}

void wxAuiGenericToolBarArt::DrawSeparator(
                                    wxDC& dc,
                                    wxWindow* wnd,
                                    const wxRect& _rect)
{
    bool horizontal = true;
    if (m_flags & wxAUI_TB_VERTICAL)
        horizontal = false;

    wxRect rect = _rect;

    if (horizontal)
    {
        rect.x += (rect.width/2);
        rect.width = wnd->FromDIP(1);
        int new_height = (rect.height*3)/4;
        rect.y += (rect.height/2) - (new_height/2);
        rect.height = new_height;
    }
    else
    {
        rect.y += (rect.height/2);
        rect.height = wnd->FromDIP(1);
        int new_width = (rect.width*3)/4;
        rect.x += (rect.width/2) - (new_width/2);
        rect.width = new_width;
    }

    wxColour startColour = m_baseColour.ChangeLightness(IsThemeDark() ? 120 : 80);
    wxColour endColour = m_baseColour.ChangeLightness(IsThemeDark() ? 120 : 80);
    dc.GradientFillLinear(rect, startColour, endColour, horizontal ? wxSOUTH : wxEAST);
}

void wxAuiGenericToolBarArt::DrawGripper(wxDC& dc,
                                    wxWindow* wnd,
                                    const wxRect& rect)
{
    int i = 0;
    while (1)
    {
        int x, y;

        if (m_flags & wxAUI_TB_VERTICAL)
        {
            x = rect.x + (i*wnd->FromDIP(4)) + wnd->FromDIP(5);
            y = rect.y + wnd->FromDIP(3);
            if (x > rect.GetWidth()-wnd->FromDIP(5))
                break;
        }
        else
        {
            x = rect.x + wnd->FromDIP(3);
            y = rect.y + (i*wnd->FromDIP(4)) + wnd->FromDIP(5);
            if (y > rect.GetHeight()-wnd->FromDIP(5))
                break;
        }

        dc.SetPen(m_gripperPen1);
        dc.DrawPoint(x, y);
        dc.SetPen(m_gripperPen2);
        dc.DrawPoint(x                , y+wnd->FromDIP(1));
        dc.DrawPoint(x+wnd->FromDIP(1), y                );
        dc.SetPen(m_gripperPen3);
        dc.DrawPoint(x+wnd->FromDIP(2), y+wnd->FromDIP(1));
        dc.DrawPoint(x+wnd->FromDIP(2), y+wnd->FromDIP(2));
        dc.DrawPoint(x+wnd->FromDIP(1), y+wnd->FromDIP(2));

        i++;
    }

}

void wxAuiGenericToolBarArt::DrawOverflowButton(wxDC& dc,
                                          wxWindow* wnd,
                                          const wxRect& rect,
                                          int state)
{
    if (state & wxAUI_BUTTON_STATE_HOVER ||
        state & wxAUI_BUTTON_STATE_PRESSED)
    {
        wxColor light_gray_bg = m_highlightColour.ChangeLightness(IsThemeDark() ? 40 : 170);

        if (m_flags & wxAUI_TB_VERTICAL)
        {
            dc.SetPen(wxPen(m_highlightColour));
            dc.DrawLine(rect.x, rect.y, rect.x+rect.width, rect.y);
            dc.SetPen(wxPen(light_gray_bg));
            dc.SetBrush(wxBrush(light_gray_bg));
            dc.DrawRectangle(rect.x, rect.y+1, rect.width, rect.height);
        }
        else
        {
            dc.SetPen(wxPen(m_highlightColour));
            dc.DrawLine(rect.x, rect.y, rect.x, rect.y+rect.height);
            dc.SetPen(wxPen(light_gray_bg));
            dc.SetBrush(wxBrush(light_gray_bg));
            dc.DrawRectangle(rect.x+1, rect.y, rect.width, rect.height);
        }
    }

    const wxBitmap overflowBmp = m_overflowBmp.GetBitmapFor(wnd);
    int x = rect.x+1+(rect.width-overflowBmp.GetLogicalWidth())/2;
    int y = rect.y+1+(rect.height-overflowBmp.GetLogicalHeight())/2;
    dc.DrawBitmap(overflowBmp, x, y, true);
}

int wxAuiGenericToolBarArt::GetElementSize(int element_id)
{
    switch (element_id)
    {
        case wxAUI_TBART_SEPARATOR_SIZE: return m_separatorSize;
        case wxAUI_TBART_GRIPPER_SIZE:   return m_gripperSize;
        case wxAUI_TBART_OVERFLOW_SIZE:  return m_overflowSize;
        case wxAUI_TBART_DROPDOWN_SIZE:  return m_dropdownSize;
        default: return 0;
    }
}

void wxAuiGenericToolBarArt::SetElementSize(int element_id, int size)
{
    switch (element_id)
    {
        case wxAUI_TBART_SEPARATOR_SIZE: m_separatorSize = size; break;
        case wxAUI_TBART_GRIPPER_SIZE:   m_gripperSize = size; break;
        case wxAUI_TBART_OVERFLOW_SIZE:  m_overflowSize = size; break;
        case wxAUI_TBART_DROPDOWN_SIZE:  m_dropdownSize = size; break;
    }
}

int wxAuiGenericToolBarArt::ShowDropDown(wxWindow* wnd,
                                         const wxAuiToolBarItemArray& items)
{
    wxMenu menuPopup;

    size_t items_added = 0;

    size_t i, count = items.GetCount();
    for (i = 0; i < count; ++i)
    {
        wxAuiToolBarItem& item = items.Item(i);

        if (item.GetKind() == wxITEM_NORMAL)
        {
            wxString text = item.GetShortHelp();
            if (text.empty())
                text = item.GetLabel();

            if (text.empty())
                text = wxT(" ");

            wxMenuItem* m =  new wxMenuItem(&menuPopup, item.GetId(), text, item.GetShortHelp());

            m->SetBitmap(item.GetBitmapBundle().GetBitmapFor(wnd));
            menuPopup.Append(m);
            items_added++;
        }
        else if (item.GetKind() == wxITEM_SEPARATOR)
        {
            if (items_added > 0)
                menuPopup.AppendSeparator();
        }
    }

    // find out where to put the popup menu of window items
    wxPoint pt = ::wxGetMousePosition();
    pt = wnd->ScreenToClient(pt);

    // find out the screen coordinate at the bottom of the tab ctrl
    wxRect cli_rect = wnd->GetClientRect();
    pt.y = cli_rect.y + cli_rect.height;

    ToolbarCommandCapture* cc = new ToolbarCommandCapture;
    wnd->PushEventHandler(cc);
    wnd->PopupMenu(&menuPopup, pt);
    int command = cc->GetCommandId();
    wnd->PopEventHandler(true);

    return command;
}




static wxOrientation GetOrientation(long style)
{
    switch (style & wxAUI_ORIENTATION_MASK)
    {
        case wxAUI_TB_HORIZONTAL:
            return wxHORIZONTAL;
        case wxAUI_TB_VERTICAL:
            return wxVERTICAL;
        default:
            wxFAIL_MSG("toolbar cannot be locked in both horizontal and vertical orientations (maybe no lock was intended?)");
            wxFALLTHROUGH;
        case 0:
            return wxBOTH;
    }
}

wxBEGIN_EVENT_TABLE(wxAuiToolBar, wxControl)
    EVT_SIZE(wxAuiToolBar::OnSize)
    EVT_IDLE(wxAuiToolBar::OnIdle)
    EVT_DPI_CHANGED(wxAuiToolBar::OnDPIChanged)
    EVT_PAINT(wxAuiToolBar::OnPaint)
    EVT_LEFT_DOWN(wxAuiToolBar::OnLeftDown)
    EVT_LEFT_DCLICK(wxAuiToolBar::OnLeftDown)
    EVT_LEFT_UP(wxAuiToolBar::OnLeftUp)
    EVT_RIGHT_DOWN(wxAuiToolBar::OnRightDown)
    EVT_RIGHT_DCLICK(wxAuiToolBar::OnRightDown)
    EVT_RIGHT_UP(wxAuiToolBar::OnRightUp)
    EVT_MIDDLE_DOWN(wxAuiToolBar::OnMiddleDown)
    EVT_MIDDLE_DCLICK(wxAuiToolBar::OnMiddleDown)
    EVT_MIDDLE_UP(wxAuiToolBar::OnMiddleUp)
    EVT_MOTION(wxAuiToolBar::OnMotion)
    EVT_LEAVE_WINDOW(wxAuiToolBar::OnLeaveWindow)
    EVT_MOUSE_CAPTURE_LOST(wxAuiToolBar::OnCaptureLost)
    EVT_SET_CURSOR(wxAuiToolBar::OnSetCursor)
    EVT_SYS_COLOUR_CHANGED(wxAuiToolBar::OnSysColourChanged)
wxEND_EVENT_TABLE()

void wxAuiToolBar::Init()
{
    m_sizer = new wxBoxSizer(wxHORIZONTAL);
    m_buttonWidth = -1;
    m_buttonHeight = -1;
    m_sizerElementCount = 0;
    m_actionPos = wxDefaultPosition;
    m_actionItem = nullptr;
    m_tipItem = nullptr;
    m_art = new wxAuiDefaultToolBarArt;
    m_toolTextOrientation = wxAUI_TBTOOL_TEXT_BOTTOM;
    m_gripperSizerItem = nullptr;
    m_overflowSizerItem = nullptr;
    m_dragging = false;
    m_gripperVisible = false;
    m_overflowVisible = false;
    m_overflowState = 0;
    m_orientation = wxHORIZONTAL;
}

bool wxAuiToolBar::Create(wxWindow* parent,
                           wxWindowID id,
                           const wxPoint& pos,
                           const wxSize& size,
                           long style)
{
    style = style|wxBORDER_NONE;

    if (!wxControl::Create(parent, id, pos, size, style))
        return false;

    m_windowStyle = style;

    m_toolPacking = FromDIP(2);
    m_toolBorderPadding = FromDIP(3);

    m_gripperVisible  = (style & wxAUI_TB_GRIPPER) ? true : false;
    m_overflowVisible = (style & wxAUI_TB_OVERFLOW) ? true : false;

    m_orientation = GetOrientation(style);
    if (m_orientation == wxBOTH)
    {
        m_orientation = wxHORIZONTAL;
    }

    wxSize margin_lt = FromDIP(wxSize(5, 5));
    wxSize margin_rb = FromDIP(wxSize(2, 2));
    SetMargins(margin_lt.x, margin_lt.y, margin_rb.x, margin_rb.y);
    SetFont(*wxNORMAL_FONT);
    SetArtFlags();
    SetExtraStyle(wxWS_EX_PROCESS_IDLE);
    if (style & wxAUI_TB_HORZ_LAYOUT)
        SetToolTextOrientation(wxAUI_TBTOOL_TEXT_RIGHT);

    return true;
}

wxAuiToolBar::~wxAuiToolBar()
{
    delete m_art;
    delete m_sizer;
}

void wxAuiToolBar::SetWindowStyleFlag(long style)
{
    GetOrientation(style);      // assert if style is invalid
    wxCHECK_RET(IsPaneValid(style),
                "window settings and pane settings are incompatible");

    const auto oldBgStyle = m_windowStyle & wxAUI_TB_PLAIN_BACKGROUND;

    wxControl::SetWindowStyleFlag(style);

    if (m_art)
    {
        SetArtFlags();
    }

    if (m_windowStyle & wxAUI_TB_GRIPPER)
        m_gripperVisible = true;
    else
        m_gripperVisible = false;


    if (m_windowStyle & wxAUI_TB_OVERFLOW)
        m_overflowVisible = true;
    else
        m_overflowVisible = false;

    if (style & wxAUI_TB_HORZ_LAYOUT)
        SetToolTextOrientation(wxAUI_TBTOOL_TEXT_RIGHT);
    else
        SetToolTextOrientation(wxAUI_TBTOOL_TEXT_BOTTOM);

    wxControl::SetWindowStyleFlag(style);

    const auto newBgStyle = m_windowStyle & wxAUI_TB_PLAIN_BACKGROUND;

    if ( newBgStyle != oldBgStyle )
        UpdateBackgroundBitmap(GetClientSize());
}

wxSize wxAuiToolBar::DoGetBestSize() const
{
    auto bestSize = GetMinSize();

    if ( !bestSize.IsFullySpecified() )
    {
        bestSize.SetDefaults(FromDIP(wxSize(1, 1)));
    }

    return bestSize;
}

void wxAuiToolBar::SetArtProvider(wxAuiToolBarArt* art)
{
    delete m_art;

    m_art = art;

    if (m_art)
    {
        SetArtFlags();
        m_art->SetTextOrientation(m_toolTextOrientation);
    }
}

wxAuiToolBarArt* wxAuiToolBar::GetArtProvider() const
{
    return m_art;
}




wxAuiToolBarItem* wxAuiToolBar::AddTool(int tool_id,
                           const wxString& label,
                           const wxBitmapBundle& bitmap,
                           const wxString& shortHelp_string,
                           wxItemKind kind)
{
    return AddTool(tool_id,
            label,
            bitmap,
            wxBitmapBundle(),
            kind,
            shortHelp_string,
            wxEmptyString,
            nullptr);
}


wxAuiToolBarItem* wxAuiToolBar::AddTool(int tool_id,
                           const wxString& label,
                           const wxBitmapBundle& bitmap,
                           const wxBitmapBundle& disabledBitmap,
                           wxItemKind kind,
                           const wxString& shortHelpString,
                           const wxString& longHelpString,
                           wxObject* client_data)
{
    wxAuiToolBarItem item;
    item.m_window = nullptr;
    item.m_label = label;
    item.m_bitmap = bitmap;
    item.m_disabledBitmap = disabledBitmap;
    item.m_shortHelp = shortHelpString;
    item.m_longHelp = longHelpString;
    item.m_active = true;
    item.m_dropDown = false;
    item.m_spacerPixels = 0;
    item.m_toolId = tool_id;
    item.m_state = 0;
    item.m_proportion = 0;
    item.m_kind = kind;
    item.m_sizerItem = nullptr;
    item.m_minSize = wxDefaultSize;
    item.m_userData = 0;
    item.m_clientData = client_data;
    item.m_sticky = false;

    if (item.m_toolId == wxID_ANY)
        item.m_toolId = wxNewId();

    m_items.Add(item);
    return &m_items.Last();
}

wxAuiToolBarItem* wxAuiToolBar::AddControl(wxControl* control,
                              const wxString& label)
{
    wxAuiToolBarItem item;
    item.m_window = (wxWindow*)control;
    item.m_label = label;
    item.m_bitmap = wxBitmapBundle();
    item.m_disabledBitmap = wxBitmapBundle();
    item.m_active = true;
    item.m_dropDown = false;
    item.m_spacerPixels = 0;
    item.m_toolId = control->GetId();
    item.m_state = 0;
    item.m_proportion = 0;
    item.m_kind = wxITEM_CONTROL;
    item.m_sizerItem = nullptr;
    item.m_minSize = control->GetEffectiveMinSize();
    item.m_userData = 0;
    item.m_sticky = false;

    m_items.Add(item);
    return &m_items.Last();
}

wxAuiToolBarItem* wxAuiToolBar::AddLabel(int tool_id,
                            const wxString& label,
                            const int width)
{
    wxSize min_size = wxDefaultSize;
    if (width != -1)
        min_size.x = width;

    wxAuiToolBarItem item;
    item.m_window = nullptr;
    item.m_label = label;
    item.m_bitmap = wxBitmapBundle();
    item.m_disabledBitmap = wxBitmapBundle();
    item.m_active = true;
    item.m_dropDown = false;
    item.m_spacerPixels = 0;
    item.m_toolId = tool_id;
    item.m_state = 0;
    item.m_proportion = 0;
    item.m_kind = wxITEM_LABEL;
    item.m_sizerItem = nullptr;
    item.m_minSize = min_size;
    item.m_userData = 0;
    item.m_sticky = false;

    if (item.m_toolId == wxID_ANY)
        item.m_toolId = wxNewId();

    m_items.Add(item);
    return &m_items.Last();
}

wxAuiToolBarItem* wxAuiToolBar::AddSeparator()
{
    wxAuiToolBarItem item;
    item.m_window = nullptr;
    item.m_label = wxEmptyString;
    item.m_bitmap = wxBitmapBundle();
    item.m_disabledBitmap = wxBitmapBundle();
    item.m_active = true;
    item.m_dropDown = false;
    item.m_toolId = -1;
    item.m_state = 0;
    item.m_proportion = 0;
    item.m_kind = wxITEM_SEPARATOR;
    item.m_sizerItem = nullptr;
    item.m_minSize = wxDefaultSize;
    item.m_userData = 0;
    item.m_sticky = false;

    m_items.Add(item);
    return &m_items.Last();
}

wxAuiToolBarItem* wxAuiToolBar::AddSpacer(int pixels)
{
    wxAuiToolBarItem item;
    item.m_window = nullptr;
    item.m_label = wxEmptyString;
    item.m_bitmap = wxBitmapBundle();
    item.m_disabledBitmap = wxBitmapBundle();
    item.m_active = true;
    item.m_dropDown = false;
    item.m_spacerPixels = pixels;
    item.m_toolId = -1;
    item.m_state = 0;
    item.m_proportion = 0;
    item.m_kind = wxITEM_SPACER;
    item.m_sizerItem = nullptr;
    item.m_minSize = wxDefaultSize;
    item.m_userData = 0;
    item.m_sticky = false;

    m_items.Add(item);
    return &m_items.Last();
}

wxAuiToolBarItem* wxAuiToolBar::AddStretchSpacer(int proportion)
{
    wxAuiToolBarItem item;
    item.m_window = nullptr;
    item.m_label = wxEmptyString;
    item.m_bitmap = wxBitmapBundle();
    item.m_disabledBitmap = wxBitmapBundle();
    item.m_active = true;
    item.m_dropDown = false;
    item.m_spacerPixels = 0;
    item.m_toolId = -1;
    item.m_state = 0;
    item.m_proportion = proportion;
    item.m_kind = wxITEM_SPACER;
    item.m_sizerItem = nullptr;
    item.m_minSize = wxDefaultSize;
    item.m_userData = 0;
    item.m_sticky = false;

    m_items.Add(item);
    return &m_items.Last();
}

void wxAuiToolBar::Clear()
{
    m_items.Clear();
    m_sizerElementCount = 0;
}

bool wxAuiToolBar::DeleteTool(int tool_id)
{
    return DeleteByIndex(GetToolIndex(tool_id));
}

bool wxAuiToolBar::DeleteByIndex(int idx)
{
    if (idx >= 0 && idx < (int)m_items.GetCount())
    {
        m_items.RemoveAt(idx);
        Realize();
        return true;
    }

    return false;
}

bool wxAuiToolBar::DestroyTool(int tool_id)
{
    return DestroyToolByIndex(GetToolIndex(tool_id));
}

bool wxAuiToolBar::DestroyToolByIndex(int idx)
{
    if ( idx < 0 || static_cast<unsigned>(idx) >= m_items.GetCount() )
        return false;

    if ( wxWindow* window = m_items[idx].GetWindow() )
        window->Destroy();

    return DeleteByIndex(idx);
}


wxControl* wxAuiToolBar::FindControl(int id)
{
    wxWindow* wnd = FindWindow(id);
    return (wxControl*)wnd;
}

wxAuiToolBarItem* wxAuiToolBar::FindTool(int tool_id) const
{
    size_t i, count;
    for (i = 0, count = m_items.GetCount(); i < count; ++i)
    {
        wxAuiToolBarItem& item = m_items.Item(i);
        if (item.m_toolId == tool_id)
            return &item;
    }

    return nullptr;
}

wxAuiToolBarItem* wxAuiToolBar::FindToolByPosition(wxCoord x, wxCoord y) const
{
    size_t i, count;
    for (i = 0, count = m_items.GetCount(); i < count; ++i)
    {
        wxAuiToolBarItem& item = m_items.Item(i);

        if (!item.m_sizerItem)
            continue;

        wxRect rect = item.m_sizerItem->GetRect();
        if (rect.Contains(x,y))
        {
            // if the item doesn't fit on the toolbar, return nullptr
            if (!GetToolFitsByIndex(i))
                return nullptr;

            return &item;
        }
    }

    return nullptr;
}

wxAuiToolBarItem* wxAuiToolBar::FindToolByPositionWithPacking(wxCoord x, wxCoord y) const
{
    size_t i, count;
    for (i = 0, count = m_items.GetCount(); i < count; ++i)
    {
        wxAuiToolBarItem& item = m_items.Item(i);

        if (!item.m_sizerItem)
            continue;

        wxRect rect = item.m_sizerItem->GetRect();

        // apply tool packing
        if (i+1 < count)
            rect.width += m_toolPacking;

        if (rect.Contains(x,y))
        {
            // if the item doesn't fit on the toolbar, return nullptr
            if (!GetToolFitsByIndex(i))
                return nullptr;

            return &item;
        }
    }

    return nullptr;
}

wxAuiToolBarItem* wxAuiToolBar::FindToolByIndex(int idx) const
{
    if (idx < 0)
        return nullptr;

    if (idx >= (int)m_items.size())
        return nullptr;

    return const_cast<wxAuiToolBarItem*>(&(m_items[idx]));
}

void wxAuiToolBar::SetToolClientData (int tool_id, wxObject *client_data)
{
    wxAuiToolBarItem* item = FindTool(tool_id);
    if (!item)
        return;

    item->m_clientData = client_data;
}

wxObject* wxAuiToolBar::GetToolClientData(int tool_id) const
{
    wxAuiToolBarItem* item = FindTool(tool_id);
    if (!item)
        return nullptr;

    return item->m_clientData;
}

void wxAuiToolBar::SetToolBitmapSize(const wxSize& WXUNUSED(size))
{
    // TODO: wxToolBar compatibility
}

wxSize wxAuiToolBar::GetToolBitmapSize() const
{
    // TODO: wxToolBar compatibility
    return FromDIP(wxSize(16,15));
}

void wxAuiToolBar::SetToolProportion(int tool_id, int proportion)
{
    wxAuiToolBarItem* item = FindTool(tool_id);
    if (!item)
        return;

    item->m_proportion = proportion;
}

int wxAuiToolBar::GetToolProportion(int tool_id) const
{
    wxAuiToolBarItem* item = FindTool(tool_id);
    if (!item)
        return 0;

    return item->m_proportion;
}

void wxAuiToolBar::SetToolSeparation(int separation)
{
    if (m_art)
        m_art->SetElementSize(wxAUI_TBART_SEPARATOR_SIZE, separation);
}

int wxAuiToolBar::GetToolSeparation() const
{
    if (m_art)
        return m_art->GetElementSizeForWindow(wxAUI_TBART_SEPARATOR_SIZE, this);
    else
        return FromDIP(5);
}


void wxAuiToolBar::SetToolDropDown(int tool_id, bool dropdown)
{
    wxAuiToolBarItem* item = FindTool(tool_id);
    if (!item)
        return;

    item->SetHasDropDown(dropdown);
}

bool wxAuiToolBar::GetToolDropDown(int tool_id) const
{
    wxAuiToolBarItem* item = FindTool(tool_id);
    if (!item)
        return false;

    return item->HasDropDown();
}

void wxAuiToolBar::SetToolSticky(int tool_id, bool sticky)
{
    // ignore separators
    if (tool_id == -1)
        return;

    wxAuiToolBarItem* item = FindTool(tool_id);
    if (!item)
        return;

    if (item->m_sticky == sticky)
        return;

    item->m_sticky = sticky;

    Refresh();
    Update();
}

bool wxAuiToolBar::GetToolSticky(int tool_id) const
{
    wxAuiToolBarItem* item = FindTool(tool_id);
    if (!item)
        return 0;

    return item->m_sticky;
}




void wxAuiToolBar::SetToolBorderPadding(int padding)
{
    m_toolBorderPadding = padding;
}

int wxAuiToolBar::GetToolBorderPadding() const
{
    return m_toolBorderPadding;
}

void wxAuiToolBar::SetToolTextOrientation(int orientation)
{
    m_toolTextOrientation = orientation;

    if (m_art)
    {
        m_art->SetTextOrientation(orientation);
    }
}

int wxAuiToolBar::GetToolTextOrientation() const
{
    return m_toolTextOrientation;
}

void wxAuiToolBar::SetToolPacking(int packing)
{
    m_toolPacking = packing;
}

int wxAuiToolBar::GetToolPacking() const
{
    return m_toolPacking;
}


void wxAuiToolBar::SetOrientation(int orientation)
{
    wxCHECK_RET(orientation == wxHORIZONTAL ||
                orientation == wxVERTICAL,
                "invalid orientation value");
    if (orientation != m_orientation)
    {
        m_orientation = wxOrientation(orientation);
        SetArtFlags();

        Realize();
    }
}

void wxAuiToolBar::SetMargins(int left, int right, int top, int bottom)
{
    if (left != -1)
        m_leftPadding = left;
    if (right != -1)
        m_rightPadding = right;
    if (top != -1)
        m_topPadding = top;
    if (bottom != -1)
        m_bottomPadding = bottom;
}

bool wxAuiToolBar::GetGripperVisible() const
{
    return m_gripperVisible;
}

void wxAuiToolBar::SetGripperVisible(bool visible)
{
    m_gripperVisible = visible;
    if (visible)
        m_windowStyle |= wxAUI_TB_GRIPPER;
    else
        m_windowStyle &= ~wxAUI_TB_GRIPPER;
    Realize();
    Refresh();
}


bool wxAuiToolBar::GetOverflowVisible() const
{
    return m_overflowVisible;
}

void wxAuiToolBar::SetOverflowVisible(bool visible)
{
    m_overflowVisible = visible;
    if (visible)
        m_windowStyle |= wxAUI_TB_OVERFLOW;
    else
        m_windowStyle &= ~wxAUI_TB_OVERFLOW;
    Refresh();
}

bool wxAuiToolBar::SetFont(const wxFont& font)
{
    bool res = wxWindow::SetFont(font);

    if (m_art)
    {
        m_art->SetFont(font);
    }

    return res;
}


void wxAuiToolBar::SetHoverItem(wxAuiToolBarItem* pitem)
{
    if ( wxFrame* frame = wxDynamicCast(wxGetTopLevelParent(this), wxFrame) )
    {
        wxString help;
        if (pitem)
        {
            help = pitem->GetLongHelp();
        }
        frame->DoGiveHelp(help, pitem);
    }

    if (pitem && (pitem->m_state & wxAUI_BUTTON_STATE_DISABLED))
        pitem = nullptr;

    wxAuiToolBarItem* former_hover = nullptr;

    size_t i, count;
    for (i = 0, count = m_items.GetCount(); i < count; ++i)
    {
        wxAuiToolBarItem& item = m_items.Item(i);
        if (item.m_state & wxAUI_BUTTON_STATE_HOVER)
            former_hover = &item;
        item.m_state &= ~wxAUI_BUTTON_STATE_HOVER;
    }

    if (pitem)
    {
        pitem->m_state |= wxAUI_BUTTON_STATE_HOVER;
    }

    if (former_hover != pitem)
    {
        Refresh();
        Update();
    }
}

void wxAuiToolBar::SetPressedItem(wxAuiToolBarItem* pitem)
{
    wxAuiToolBarItem* former_item = nullptr;

    size_t i, count;
    for (i = 0, count = m_items.GetCount(); i < count; ++i)
    {
        wxAuiToolBarItem& item = m_items.Item(i);
        if (item.m_state & wxAUI_BUTTON_STATE_PRESSED)
            former_item = &item;
        item.m_state &= ~wxAUI_BUTTON_STATE_PRESSED;
    }

    if (pitem)
    {
        pitem->m_state &= ~wxAUI_BUTTON_STATE_HOVER;
        pitem->m_state |= wxAUI_BUTTON_STATE_PRESSED;
    }

    if (former_item != pitem)
    {
        Refresh();
        Update();
    }
}

void wxAuiToolBar::RefreshOverflowState()
{
    if (!m_overflowSizerItem)
    {
        m_overflowState = 0;
        return;
    }

    int overflow_state = 0;

    wxRect overflow_rect = GetOverflowRect();


    // find out the mouse's current position
    wxPoint pt = ::wxGetMousePosition();
    pt = this->ScreenToClient(pt);

    // find out if the mouse cursor is inside the dropdown rectangle
    if (overflow_rect.Contains(pt.x, pt.y))
    {
        if (::wxGetMouseState().LeftIsDown())
            overflow_state = wxAUI_BUTTON_STATE_PRESSED;
        else
            overflow_state = wxAUI_BUTTON_STATE_HOVER;
    }

    if (overflow_state != m_overflowState)
    {
        m_overflowState = overflow_state;
        Refresh();
        Update();
    }

    m_overflowState = overflow_state;
}

void wxAuiToolBar::ToggleTool(int tool_id, bool state)
{
    wxAuiToolBarItem* tool = FindTool(tool_id);

    if ( tool && tool->CanBeToggled() )
    {
        if (tool->m_kind == wxITEM_RADIO)
        {
            int idx, count;
            idx = GetToolIndex(tool_id);
            count = (int)m_items.GetCount();

            if (idx >= 0 && idx < count)
            {
                int i;
                for (i = idx + 1; i < count; ++i)
                {
                    if (m_items[i].m_kind != wxITEM_RADIO)
                        break;
                    m_items[i].m_state &= ~wxAUI_BUTTON_STATE_CHECKED;
                }
                for (i = idx - 1; i >= 0; i--)
                {
                    if (m_items[i].m_kind != wxITEM_RADIO)
                        break;
                    m_items[i].m_state &= ~wxAUI_BUTTON_STATE_CHECKED;
                }
            }

            tool->m_state |= wxAUI_BUTTON_STATE_CHECKED;
        }
         else if (tool->m_kind == wxITEM_CHECK)
        {
            if (state == true)
                tool->m_state |= wxAUI_BUTTON_STATE_CHECKED;
            else
                tool->m_state &= ~wxAUI_BUTTON_STATE_CHECKED;
        }
    }
}

bool wxAuiToolBar::GetToolToggled(int tool_id) const
{
    wxAuiToolBarItem* tool = FindTool(tool_id);

    if ( tool && tool->CanBeToggled() )
        return (tool->m_state & wxAUI_BUTTON_STATE_CHECKED) ? true : false;

    return false;
}

void wxAuiToolBar::EnableTool(int tool_id, bool state)
{
    wxAuiToolBarItem* tool = FindTool(tool_id);

    if (tool)
    {
        if (state == true)
            tool->m_state &= ~wxAUI_BUTTON_STATE_DISABLED;
        else
            tool->m_state |= wxAUI_BUTTON_STATE_DISABLED;
    }
}

bool wxAuiToolBar::GetToolEnabled(int tool_id) const
{
    wxAuiToolBarItem* tool = FindTool(tool_id);

    if (tool)
        return (tool->m_state & wxAUI_BUTTON_STATE_DISABLED) ? false : true;

    return false;
}

wxString wxAuiToolBar::GetToolLabel(int tool_id) const
{
    wxAuiToolBarItem* tool = FindTool(tool_id);
    wxASSERT_MSG(tool, wxT("can't find tool in toolbar item array"));
    if (!tool)
        return wxEmptyString;

    return tool->m_label;
}

void wxAuiToolBar::SetToolLabel(int tool_id, const wxString& label)
{
    wxAuiToolBarItem* tool = FindTool(tool_id);
    if (tool)
    {
        tool->m_label = label;
    }
}

wxBitmap wxAuiToolBar::GetToolBitmap(int tool_id) const
{
    wxAuiToolBarItem* tool = FindTool(tool_id);
    wxASSERT_MSG(tool, wxT("can't find tool in toolbar item array"));
    if (!tool)
        return wxNullBitmap;

    return tool->m_bitmap.GetBitmapFor(this);
}

void wxAuiToolBar::SetToolBitmap(int tool_id, const wxBitmapBundle& bitmap)
{
    wxAuiToolBarItem* tool = FindTool(tool_id);
    if (tool)
    {
        tool->m_bitmap = bitmap;
    }
}

wxString wxAuiToolBar::GetToolShortHelp(int tool_id) const
{
    wxAuiToolBarItem* tool = FindTool(tool_id);
    wxASSERT_MSG(tool, wxT("can't find tool in toolbar item array"));
    if (!tool)
        return wxEmptyString;

    return tool->m_shortHelp;
}

void wxAuiToolBar::SetToolShortHelp(int tool_id, const wxString& help_string)
{
    wxAuiToolBarItem* tool = FindTool(tool_id);
    if (tool)
    {
        tool->m_shortHelp = help_string;
    }
}

wxString wxAuiToolBar::GetToolLongHelp(int tool_id) const
{
    wxAuiToolBarItem* tool = FindTool(tool_id);
    wxASSERT_MSG(tool, wxT("can't find tool in toolbar item array"));
    if (!tool)
        return wxEmptyString;

    return tool->m_longHelp;
}

void wxAuiToolBar::SetToolLongHelp(int tool_id, const wxString& help_string)
{
    wxAuiToolBarItem* tool = FindTool(tool_id);
    if (tool)
    {
        tool->m_longHelp = help_string;
    }
}

void wxAuiToolBar::SetCustomOverflowItems(const wxAuiToolBarItemArray& prepend,
                                          const wxAuiToolBarItemArray& append)
{
    m_customOverflowPrepend = prepend;
    m_customOverflowAppend = append;
}

// get size of hint rectangle for a particular dock location
wxSize wxAuiToolBar::GetHintSize(int dock_direction) const
{
    switch (dock_direction)
    {
        case wxAUI_DOCK_TOP:
        case wxAUI_DOCK_BOTTOM:
            return m_horzHintSize;
        case wxAUI_DOCK_RIGHT:
        case wxAUI_DOCK_LEFT:
            return m_vertHintSize;
        default:
            wxFAIL_MSG("invalid dock location value");
    }
    return wxDefaultSize;
}

bool wxAuiToolBar::IsPaneValid(const wxAuiPaneInfo& pane) const
{
    return IsPaneValid(m_windowStyle, pane);
}

bool wxAuiToolBar::IsPaneValid(long style, const wxAuiPaneInfo& pane)
{
    if (style & wxAUI_TB_HORIZONTAL)
    {
        if (pane.IsLeftDockable() || pane.IsRightDockable())
        {
            return false;
        }
    }
    else if (style & wxAUI_TB_VERTICAL)
    {
        if (pane.IsTopDockable() || pane.IsBottomDockable())
        {
            return false;
        }
    }
    return true;
}

bool wxAuiToolBar::IsPaneValid(long style) const
{
    wxAuiManager* manager = wxAuiManager::GetManager(const_cast<wxAuiToolBar*>(this));
    if (manager)
    {
        return IsPaneValid(style, manager->GetPane(const_cast<wxAuiToolBar*>(this)));
    }
    return true;
}

void wxAuiToolBar::SetArtFlags() const
{
    unsigned int artflags = m_windowStyle & ~wxAUI_ORIENTATION_MASK;
    if (m_orientation == wxVERTICAL)
    {
        artflags |= wxAUI_TB_VERTICAL;
    }
    m_art->SetFlags(artflags);
}

size_t wxAuiToolBar::GetToolCount() const
{
    return m_items.size();
}

int wxAuiToolBar::GetToolIndex(int tool_id) const
{
    // this will prevent us from returning the index of the
    // first separator in the toolbar since its id is equal to -1
    if (tool_id == -1)
        return wxNOT_FOUND;

    size_t i, count = m_items.GetCount();
    for (i = 0; i < count; ++i)
    {
        wxAuiToolBarItem& item = m_items.Item(i);
        if (item.m_toolId == tool_id)
            return i;
    }

    return wxNOT_FOUND;
}

bool wxAuiToolBar::GetToolFitsByIndex(int tool_idx) const
{
    if (tool_idx < 0 || tool_idx >= (int)m_items.GetCount())
        return false;

    if (!m_items[tool_idx].m_sizerItem)
        return false;

    int cli_w, cli_h;
    GetClientSize(&cli_w, &cli_h);

    wxRect rect = m_items[tool_idx].m_sizerItem->GetRect();

    if (m_orientation == wxVERTICAL)
    {
        // take the dropdown size into account
        if (m_overflowVisible && m_overflowSizerItem)
            cli_h -= m_overflowSizerItem->GetMinSize().y;

        if (rect.y+rect.height < cli_h)
            return true;
    }
    else
    {
        // take the dropdown size into account
        if (m_overflowVisible && m_overflowSizerItem)
            cli_w -= m_overflowSizerItem->GetMinSize().x;

        if (rect.x+rect.width < cli_w)
            return true;
    }

    return false;
}


bool wxAuiToolBar::GetToolFits(int tool_id) const
{
    return GetToolFitsByIndex(GetToolIndex(tool_id));
}

wxRect wxAuiToolBar::GetToolRect(int tool_id) const
{
    wxAuiToolBarItem* tool = FindTool(tool_id);
    if (tool && tool->m_sizerItem)
    {
        return tool->m_sizerItem->GetRect();
    }

    return wxRect();
}

bool wxAuiToolBar::GetToolBarFits() const
{
    if (m_items.GetCount() == 0)
    {
        // empty toolbar always 'fits'
        return true;
    }

    // entire toolbar content fits if the last tool fits
    return GetToolFitsByIndex(m_items.GetCount() - 1);
}

bool wxAuiToolBar::Realize()
{
    wxInfoDC dc(this);
    if (!dc.IsOk())
        return false;

    // calculate hint sizes for both horizontal and vertical orientations if we
    // can use them, i.e. if the toolbar isn't locked into just one of them, and
    // store the size appropriate for the current orientation in this variable.
    wxSize size;
    if (m_orientation == wxHORIZONTAL)
    {
        if (!HasFlag(wxAUI_TB_HORIZONTAL))
            m_vertHintSize = RealizeHelper(dc, wxVERTICAL);

        m_horzHintSize = RealizeHelper(dc, wxHORIZONTAL);

        size = m_horzHintSize;
    }
    else
    {
        if (!HasFlag(wxAUI_TB_VERTICAL))
            m_horzHintSize = RealizeHelper(dc, wxHORIZONTAL);

        m_vertHintSize = RealizeHelper(dc, wxVERTICAL);

        size = m_vertHintSize;
    }

    // Remember our minimum size.
    m_minWidth = size.x;
    m_minHeight = size.y;

    // And set control size if we are not forbidden from doing it by the use of
    // a special flag and if it did actually change.
    wxSize curSize = GetClientSize();

    if ((m_windowStyle & wxAUI_TB_NO_AUTORESIZE) == 0 && (size != curSize))
    {
        SetClientSize(size);
    }
    else // Don't change the size but let the sizer know about available size.
    {
        m_sizer->SetDimension(0, 0, curSize.x, curSize.y);
    }

    Refresh();
    return true;
}

wxSize wxAuiToolBar::RealizeHelper(wxReadOnlyDC& dc, wxOrientation orientation)
{
    // Remove old sizer before adding any controls in this tool bar, which are
    // elements of this sizer, to the new sizer below.
    delete m_sizer;
    m_sizer = nullptr;

    // create the new sizer to add toolbar elements to
    wxBoxSizer* sizer = new wxBoxSizer(orientation);

    // add gripper area
    int separatorSize = m_art->GetElementSizeForWindow(wxAUI_TBART_SEPARATOR_SIZE, this);
    int gripperSize = m_art->GetElementSizeForWindow(wxAUI_TBART_GRIPPER_SIZE, this);
    if (gripperSize > 0 && m_gripperVisible)
    {
        m_gripperSizerItem = sizer->AddSpacer(gripperSize);
        m_gripperSizerItem->SetFlag(wxEXPAND);
    }
    else
    {
        m_gripperSizerItem = nullptr;
    }

    // add "left" padding
    if (m_leftPadding > 0)
    {
        sizer->AddSpacer(m_leftPadding);
    }

    size_t i, count;
    for (i = 0, count = m_items.GetCount(); i < count; ++i)
    {
        wxAuiToolBarItem& item = m_items.Item(i);
        wxSizerItem* sizerItem = nullptr;

        switch (item.m_kind)
        {
            case wxITEM_LABEL:
            {
                wxSize size = m_art->GetLabelSize(dc, this, item);
                sizerItem = sizer->Add(size.x + (m_toolBorderPadding*2),
                                        size.y + (m_toolBorderPadding*2),
                                        item.m_proportion,
                                        item.m_alignment);
                if (i+1 < count)
                {
                    sizer->AddSpacer(m_toolPacking);
                }

                break;
            }

            case wxITEM_CHECK:
            case wxITEM_NORMAL:
            case wxITEM_RADIO:
            {
                wxSize size = m_art->GetToolSize(dc, this, item);
                sizerItem = sizer->Add(size.x + (m_toolBorderPadding*2),
                                        size.y + (m_toolBorderPadding*2),
                                        0,
                                        item.m_alignment);
                // add tool packing
                if (i+1 < count)
                {
                    sizer->AddSpacer(m_toolPacking);
                }

                break;
            }

            case wxITEM_SEPARATOR:
            {
                sizerItem = sizer->AddSpacer(separatorSize);
                sizerItem->SetFlag(wxEXPAND);

                // add tool packing
                if (i+1 < count)
                {
                    sizer->AddSpacer(m_toolPacking);
                }

                break;
            }

            case wxITEM_SPACER:
                if (item.m_proportion > 0)
                    sizerItem = sizer->AddStretchSpacer(item.m_proportion);
                else
                    sizerItem = sizer->Add(item.m_spacerPixels, 1);
                break;

            case wxITEM_CONTROL:
            {
                wxSizerItem* ctrl_m_sizerItem;

                wxBoxSizer* vert_sizer = new wxBoxSizer(wxVERTICAL);
                vert_sizer->AddStretchSpacer(1);
                ctrl_m_sizerItem = vert_sizer->Add(item.m_window, 0, wxEXPAND);
                vert_sizer->AddStretchSpacer(1);
                if ( (m_windowStyle & wxAUI_TB_TEXT) &&
                     m_toolTextOrientation == wxAUI_TBTOOL_TEXT_BOTTOM &&
                     !item.GetLabel().empty() )
                {
                    wxSize s = GetLabelSize(item.GetLabel());
                    vert_sizer->Add(1, s.y);
                }


                sizerItem = sizer->Add(vert_sizer, item.m_proportion, wxEXPAND);

                wxSize min_size = item.m_minSize;


                // proportional items will disappear from the toolbar if
                // their min width is not set to something really small
                if (item.m_proportion != 0)
                {
                    min_size.x = 1;
                }

                if (min_size.IsFullySpecified())
                {
                    sizerItem->SetMinSize(min_size);
                    ctrl_m_sizerItem->SetMinSize(min_size);
                }

                // add tool packing
                if (i+1 < count)
                {
                    sizer->AddSpacer(m_toolPacking);
                }
            }
        }

        item.m_sizerItem = sizerItem;
    }

    // add "right" padding
    if (m_rightPadding > 0)
    {
        sizer->AddSpacer(m_rightPadding);
    }

    // add drop down area
    m_overflowSizerItem = nullptr;

    if (m_windowStyle & wxAUI_TB_OVERFLOW)
    {
        int overflow_size = m_art->GetElementSizeForWindow(wxAUI_TBART_OVERFLOW_SIZE, this);
        if (overflow_size > 0 && m_overflowVisible)
        {
            m_overflowSizerItem = sizer->AddSpacer(overflow_size);
            m_overflowSizerItem->SetFlag(wxEXPAND);
            m_overflowSizerItem->SetMinSize(m_overflowSizerItem->GetSize());
        }
        else
        {
            m_overflowSizerItem = nullptr;
        }
    }


    // the outside sizer helps us apply the "top" and "bottom" padding
    wxBoxSizer* outside_sizer = new wxBoxSizer(orientation ^ wxBOTH);

    // add "top" padding
    if (m_topPadding > 0)
    {
        outside_sizer->AddSpacer(m_topPadding);
    }

    // add the sizer that contains all of the toolbar elements
    outside_sizer->Add(sizer, 1, wxEXPAND);

    // add "bottom" padding
    if (m_bottomPadding > 0)
    {
        outside_sizer->AddSpacer(m_bottomPadding);
    }

    m_sizer = outside_sizer;

    // calculate the rock-bottom minimum size
    for (i = 0, count = m_items.GetCount(); i < count; ++i)
    {
        wxAuiToolBarItem& item = m_items.Item(i);
        if (item.m_sizerItem && item.m_proportion > 0 && item.m_minSize.IsFullySpecified())
            item.m_sizerItem->SetMinSize(0,0);
    }

    m_absoluteMinSize = m_sizer->GetMinSize();

    // reset the min sizes to what they were
    for (i = 0, count = m_items.GetCount(); i < count; ++i)
    {
        wxAuiToolBarItem& item = m_items.Item(i);
        if (item.m_sizerItem && item.m_proportion > 0 && item.m_minSize.IsFullySpecified())
            item.m_sizerItem->SetMinSize(item.m_minSize);
    }

    return m_sizer->GetMinSize();
}

bool wxAuiToolBar::RealizeHelper(wxClientDC& dc, bool horizontal)
{
    RealizeHelper(static_cast<wxReadOnlyDC&>(dc),
                  horizontal ? wxHORIZONTAL : wxVERTICAL);
    return true;
}

int wxAuiToolBar::GetOverflowState() const
{
    return m_overflowState;
}

wxRect wxAuiToolBar::GetOverflowRect() const
{
    wxRect cli_rect(wxPoint(0,0), GetClientSize());
    wxRect overflow_rect = m_overflowSizerItem->GetRect();
    int overflow_size = m_art->GetElementSizeForWindow(wxAUI_TBART_OVERFLOW_SIZE, this);

    if (m_orientation == wxVERTICAL)
    {
        overflow_rect.y = cli_rect.height - overflow_size;
        overflow_rect.x = 0;
        overflow_rect.width = cli_rect.width;
        overflow_rect.height = overflow_size;
    }
    else
    {
        overflow_rect.x = cli_rect.width - overflow_size;
        overflow_rect.y = 0;
        overflow_rect.width = overflow_size;
        overflow_rect.height = cli_rect.height;
    }

    return overflow_rect;
}

wxSize wxAuiToolBar::GetLabelSize(const wxString& label)
{
    wxInfoDC dc(this);

    int tx, ty;
    int textWidth = 0, textHeight = 0;

    dc.SetFont(m_font);

    // get the text height
    dc.GetTextExtent(wxT("ABCDHgj"), &tx, &textHeight);

    // get the text width
    dc.GetTextExtent(label, &textWidth, &ty);

    return wxSize(textWidth, textHeight);
}


void wxAuiToolBar::DoIdleUpdate()
{
    wxEvtHandler* handler = GetEventHandler();

    bool need_refresh = false;

    size_t i, count;
    for (i = 0, count = m_items.GetCount(); i < count; ++i)
    {
        wxAuiToolBarItem& item = m_items.Item(i);

        if (item.m_toolId == -1)
            continue;

        if (item.GetKind() != wxITEM_CONTROL)
        {
            wxUpdateUIEvent evt(item.m_toolId);
            evt.SetEventObject(this);

            if ( !item.CanBeToggled() )
                evt.DisallowCheck();

            if (handler->ProcessEvent(evt))
            {
                if (evt.GetSetEnabled())
                {
                    bool is_enabled;
                    if (item.m_window)
                        is_enabled = item.m_window->IsThisEnabled();
                    else
                        is_enabled = (item.m_state & wxAUI_BUTTON_STATE_DISABLED) ? false : true;

                    bool new_enabled = evt.GetEnabled();
                    if (new_enabled != is_enabled)
                    {
                        if (item.m_window)
                        {
                            item.m_window->Enable(new_enabled);
                        }
                        else
                        {
                            if (new_enabled)
                                item.m_state &= ~wxAUI_BUTTON_STATE_DISABLED;
                            else
                                item.m_state |= wxAUI_BUTTON_STATE_DISABLED;
                        }
                        need_refresh = true;
                    }
                }

                if (evt.GetSetChecked())
                {
                    // make sure we aren't checking an item that can't be
                    if (item.m_kind != wxITEM_CHECK && item.m_kind != wxITEM_RADIO)
                        continue;

                    bool is_checked = (item.m_state & wxAUI_BUTTON_STATE_CHECKED) ? true : false;
                    bool new_checked = evt.GetChecked();

                    if (new_checked != is_checked)
                    {
                        if (new_checked)
                            item.m_state |= wxAUI_BUTTON_STATE_CHECKED;
                        else
                            item.m_state &= ~wxAUI_BUTTON_STATE_CHECKED;

                        need_refresh = true;
                    }
                }

            }
        }
        else
        {
            item.GetWindow()->UpdateWindowUI();
        }
    }


    if (need_refresh)
    {
        Refresh();
    }
}


void wxAuiToolBar::OnSize(wxSizeEvent& WXUNUSED(evt))
{
    int x, y;
    GetClientSize(&x, &y);

    if (((x >= y) && m_absoluteMinSize.x > x) ||
        ((y > x) && m_absoluteMinSize.y > y))
    {
        // hide all flexible items
        size_t i, count;
        for (i = 0, count = m_items.GetCount(); i < count; ++i)
        {
            wxAuiToolBarItem& item = m_items.Item(i);
            if (item.m_sizerItem && item.m_proportion > 0 && item.m_sizerItem->IsShown())
            {
                item.m_sizerItem->Show(false);
                item.m_sizerItem->SetProportion(0);
            }
        }
    }
    else
    {
        // show all flexible items
        size_t i, count;
        for (i = 0, count = m_items.GetCount(); i < count; ++i)
        {
            wxAuiToolBarItem& item = m_items.Item(i);
            if (item.m_sizerItem && item.m_proportion > 0 && !item.m_sizerItem->IsShown())
            {
                item.m_sizerItem->Show(true);
                item.m_sizerItem->SetProportion(item.m_proportion);
            }
        }
    }

    m_sizer->SetDimension(0, 0, x, y);

    // We need to update the bitmap if the size has changed.
    UpdateBackgroundBitmap(wxSize(x, y));

    Refresh();

    // idle events aren't sent while user is resizing frame (why?),
    // but resizing toolbar here causes havoc,
    // so force idle handler to run after size handling complete
    QueueEvent(new wxIdleEvent);
}



void wxAuiToolBar::OnIdle(wxIdleEvent& evt)
{
    // if orientation doesn't match dock, fix it
    wxAuiManager* manager = wxAuiManager::GetManager(this);
    if (manager)
    {
        wxAuiPaneInfo& pane = manager->GetPane(this);
        // pane state member is public, so it might have been changed
        // without going through wxPaneInfo::SetFlag() check
        bool ok = pane.IsOk();
        wxCHECK2_MSG(!ok || IsPaneValid(m_windowStyle, pane), ok = false,
                    "window settings and pane settings are incompatible");
        if (ok)
        {
            wxOrientation newOrientation = m_orientation;
            if (pane.IsDocked())
            {
                switch (pane.dock_direction)
                {
                    case wxAUI_DOCK_TOP:
                    case wxAUI_DOCK_BOTTOM:
                        newOrientation = wxHORIZONTAL;
                        break;
                    case wxAUI_DOCK_LEFT:
                    case wxAUI_DOCK_RIGHT:
                        newOrientation = wxVERTICAL;
                        break;
                    default:
                        wxFAIL_MSG("invalid dock location value");
                }
            }
            else if (pane.IsResizable() &&
                    GetOrientation(m_windowStyle) == wxBOTH)
            {
                // changing orientation in OnSize causes havoc
                int x, y;
                GetClientSize(&x, &y);

                if (x > y)
                {
                    newOrientation = wxHORIZONTAL;
                }
                else
                {
                    newOrientation = wxVERTICAL;
                }
            }
            if (newOrientation != m_orientation)
            {
                SetOrientation(newOrientation);

                if (newOrientation == wxHORIZONTAL)
                {
                    pane.best_size = GetHintSize(wxAUI_DOCK_TOP);
                }
                else
                {
                    pane.best_size = GetHintSize(wxAUI_DOCK_LEFT);
                }
                if (pane.IsDocked())
                {
                    pane.floating_size = wxDefaultSize;
                    pane.floating_client_size = wxDefaultSize;
                }
                else
                {
                    SetSize(GetParent()->GetClientSize());
                }
                manager->Update();
            }
        }
    }
    evt.Skip();
}

void wxAuiToolBar::OnDPIChanged(wxDPIChangedEvent& event)
{
    Realize();

    event.Skip();
}

void wxAuiToolBar::UpdateWindowUI(long flags)
{
    if ( flags & wxUPDATE_UI_FROMIDLE )
    {
        DoIdleUpdate();
    }

    wxControl::UpdateWindowUI(flags);
}

void wxAuiToolBar::OnSysColourChanged(wxSysColourChangedEvent& event)
{
    event.Skip();

    m_art->UpdateColoursFromSystem();
    Refresh();
}

void wxAuiToolBar::UpdateBackgroundBitmap(const wxSize& size)
{
    // We can't create 0-sized bitmaps, but we can be called with 0 size: just
    // ignore it, as we'll be called again when the window is resized.
    if ( size.IsEmpty() )
        return;

    m_backgroundBitmap.Create(size);

    wxMemoryDC dc(m_backgroundBitmap);

    wxRect rect{size};

    if (m_windowStyle & wxAUI_TB_PLAIN_BACKGROUND)
        m_art->DrawPlainBackground(dc, this, rect);
    else
        m_art->DrawBackground(dc, this, rect);

    SetBackgroundBitmap(m_backgroundBitmap);
}

void wxAuiToolBar::OnPaint(wxPaintEvent& WXUNUSED(evt))
{
    wxPaintDC dc(this);
    wxRect cli_rect(wxPoint(0,0), GetClientSize());


    bool horizontal = m_orientation == wxHORIZONTAL;

    int gripperSize = m_art->GetElementSizeForWindow(wxAUI_TBART_GRIPPER_SIZE, this);
    int overflowSize = m_art->GetElementSizeForWindow(wxAUI_TBART_OVERFLOW_SIZE, this);

    // paint the gripper
    if (gripperSize > 0 && m_gripperSizerItem)
    {
        wxRect gripper_rect = m_gripperSizerItem->GetRect();
        if (horizontal)
            gripper_rect.width = gripperSize;
        else
            gripper_rect.height = gripperSize;
        m_art->DrawGripper(dc, this, gripper_rect);
    }

    // calculated how far we can draw items
    int last_extent;
    if (horizontal)
        last_extent = cli_rect.width;
    else
        last_extent = cli_rect.height;
    if (m_overflowVisible)
        last_extent -= overflowSize;

    // paint each individual tool
    size_t i, count = m_items.GetCount();
    for (i = 0; i < count; ++i)
    {
        wxAuiToolBarItem& item = m_items.Item(i);

        if (!item.m_sizerItem)
            continue;

        wxRect item_rect = item.m_sizerItem->GetRect();


        if ((horizontal  && item_rect.x + item_rect.width >= last_extent) ||
            (!horizontal && item_rect.y + item_rect.height >= last_extent))
        {
            break;
        }

        switch ( item.m_kind )
        {
            case wxITEM_NORMAL:
                // draw a regular or dropdown button
                if (!item.m_dropDown)
                    m_art->DrawButton(dc, this, item, item_rect);
                else
                    m_art->DrawDropDownButton(dc, this, item, item_rect);
                break;

            case wxITEM_CHECK:
            case wxITEM_RADIO:
                // draw a toggle button
                m_art->DrawButton(dc, this, item, item_rect);
                break;

            case wxITEM_SEPARATOR:
                // draw a separator
                m_art->DrawSeparator(dc, this, item_rect);
                break;

            case wxITEM_LABEL:
                // draw a text label only
                m_art->DrawLabel(dc, this, item, item_rect);
                break;

            case wxITEM_CONTROL:
                // draw the control's label
                m_art->DrawControlLabel(dc, this, item, item_rect);
                break;
        }

        // fire a signal to see if the item wants to be custom-rendered
        OnCustomRender(dc, item, item_rect);
    }

    // paint the overflow button
    if (overflowSize > 0 && m_overflowSizerItem && m_overflowVisible)
    {
        wxRect dropDownRect = GetOverflowRect();
        m_art->DrawOverflowButton(dc, this, dropDownRect, m_overflowState);
    }
}

void wxAuiToolBar::OnLeftDown(wxMouseEvent& evt)
{
    if (m_gripperSizerItem)
    {
        wxRect gripper_rect = m_gripperSizerItem->GetRect();
        if (gripper_rect.Contains(evt.GetX(), evt.GetY()))
        {
            // find aui manager
            wxAuiManager* manager = wxAuiManager::GetManager(this);
            if (!manager)
                return;

            int x_drag_offset = evt.GetX() - gripper_rect.GetX();
            int y_drag_offset = evt.GetY() - gripper_rect.GetY();

            // gripper was clicked
            manager->StartPaneDrag(this, wxPoint(x_drag_offset, y_drag_offset));
            return;
        }
    }

    if (m_overflowSizerItem && m_overflowVisible && m_art)
    {
        wxRect overflow_rect = GetOverflowRect();

        if (overflow_rect.Contains(evt.m_x, evt.m_y))
        {
            wxAuiToolBarEvent e(wxEVT_AUITOOLBAR_OVERFLOW_CLICK, -1);
            e.SetEventObject(this);
            e.SetToolId(-1);
            e.SetClickPoint(wxPoint(evt.GetX(), evt.GetY()));
            bool processed = GetEventHandler()->ProcessEvent(e);

            if (processed)
            {
                DoIdleUpdate();
            }
            else
            {
                size_t i, count;
                wxAuiToolBarItemArray overflow_items;


                // add custom overflow prepend items, if any
                count = m_customOverflowPrepend.GetCount();
                for (i = 0; i < count; ++i)
                    overflow_items.Add(m_customOverflowPrepend[i]);

                // only show items that don't fit in the dropdown
                count = m_items.GetCount();
                for (i = 0; i < count; ++i)
                {
                    if (!GetToolFitsByIndex(i))
                        overflow_items.Add(m_items[i]);
                }

                // add custom overflow append items, if any
                count = m_customOverflowAppend.GetCount();
                for (i = 0; i < count; ++i)
                    overflow_items.Add(m_customOverflowAppend[i]);

                int res = m_art->ShowDropDown(this, overflow_items);
                m_overflowState = 0;
                Refresh();
                if (res != -1)
                {
                    wxCommandEvent event(wxEVT_MENU, res);
                    event.SetEventObject(this);
                    GetEventHandler()->ProcessEvent(event);
                }
            }

            return;
        }
    }

    m_dragging = false;
    m_actionPos = wxPoint(evt.GetX(), evt.GetY());
    m_actionItem = FindToolByPosition(evt.GetX(), evt.GetY());

    if (m_actionItem)
    {
        if (m_actionItem->m_state & wxAUI_BUTTON_STATE_DISABLED)
        {
            m_actionPos = wxPoint(-1,-1);
            m_actionItem = nullptr;
            return;
        }

        UnsetToolTip();

        // fire the tool dropdown event
        wxAuiToolBarEvent e(wxEVT_AUITOOLBAR_TOOL_DROPDOWN, m_actionItem->m_toolId);
        e.SetEventObject(this);
        e.SetToolId(m_actionItem->m_toolId);

        int mouse_x = evt.GetX();
        wxRect rect = m_actionItem->m_sizerItem->GetRect();
        int dropdownWidth = m_art->GetElementSizeForWindow(wxAUI_TBART_DROPDOWN_SIZE, this);
        const bool dropDownHit = m_actionItem->m_dropDown &&
                                 mouse_x >= (rect.x+rect.width-dropdownWidth) &&
                                 mouse_x < (rect.x+rect.width);
        e.SetDropDownClicked(dropDownHit);

        e.SetClickPoint(evt.GetPosition());
        e.SetItemRect(rect);

        // we only set the 'pressed button' state if we hit the actual button
        // and not just the drop-down
        SetPressedItem(dropDownHit ? nullptr : m_actionItem);

        if(dropDownHit)
        {
            m_actionPos = wxPoint(-1,-1);
            m_actionItem = nullptr;
        }

        if(!GetEventHandler()->ProcessEvent(e) || e.GetSkipped())
            CaptureMouse();

        // Ensure hovered item is really ok, as mouse may have moved during
        //  event processing
        wxPoint cursor_pos_after_evt = ScreenToClient(wxGetMousePosition());
        SetHoverItem(FindToolByPosition(cursor_pos_after_evt.x, cursor_pos_after_evt.y));

        DoIdleUpdate();
    }
}

void wxAuiToolBar::OnLeftUp(wxMouseEvent& evt)
{
    if (!HasCapture())
        return;

    SetPressedItem(nullptr);

    wxAuiToolBarItem* hitItem;
    hitItem = FindToolByPosition(evt.GetX(), evt.GetY());
    SetHoverItem(hitItem);

    if (m_dragging)
    {
        // TODO: it would make sense to send out an 'END_DRAG' event here,
        // otherwise a client would never know what to do with the 'BEGIN_DRAG'
        // event

        // OnCaptureLost() will be called now and this will reset all our state
        // tracking variables
        ReleaseMouse();
    }
    else
    {
        if (m_actionItem && hitItem == m_actionItem)
        {
            UnsetToolTip();

            wxCommandEvent e(wxEVT_MENU, m_actionItem->m_toolId);
            e.SetEventObject(this);

            if (hitItem->m_kind == wxITEM_CHECK || hitItem->m_kind == wxITEM_RADIO)
            {
                const bool toggle = !(m_actionItem->m_state & wxAUI_BUTTON_STATE_CHECKED);

                ToggleTool(m_actionItem->m_toolId, toggle);

                // repaint immediately
                Refresh();
                Update();

                e.SetInt(toggle);
            }

            // we have to release the mouse *before* sending the event, because
            // we don't know what a handler might do. It could open up a popup
            // menu for example and that would make us lose our capture anyway.

            ReleaseMouse();

            GetEventHandler()->ProcessEvent(e);

            // Ensure hovered item is really ok, as mouse may have moved during
            // event processing
            wxPoint cursor_pos_after_evt = ScreenToClient(wxGetMousePosition());
            SetHoverItem(FindToolByPosition(cursor_pos_after_evt.x, cursor_pos_after_evt.y));

            DoIdleUpdate();
        }
        else
            ReleaseMouse();
    }
}

void wxAuiToolBar::OnRightDown(wxMouseEvent& evt)
{
    if (HasCapture())
        return;

    wxRect cli_rect(wxPoint(0,0), GetClientSize());

    if (m_gripperSizerItem)
    {
        wxRect gripper_rect = m_gripperSizerItem->GetRect();
        if (gripper_rect.Contains(evt.GetX(), evt.GetY()))
            return;
    }

    if (m_overflowSizerItem && m_art)
    {
        int overflowSize = m_art->GetElementSizeForWindow(wxAUI_TBART_OVERFLOW_SIZE, this);
        if (overflowSize > 0 &&
            evt.m_x > cli_rect.width - overflowSize &&
            evt.m_y >= 0 &&
            evt.m_y < cli_rect.height)
        {
            return;
        }
    }

    m_actionPos = wxPoint(evt.GetX(), evt.GetY());
    m_actionItem = FindToolByPosition(evt.GetX(), evt.GetY());

    if (m_actionItem && m_actionItem->m_state & wxAUI_BUTTON_STATE_DISABLED)
    {
        m_actionPos = wxPoint(-1,-1);
        m_actionItem = nullptr;
        return;
    }

    UnsetToolTip();
}

void wxAuiToolBar::OnRightUp(wxMouseEvent& evt)
{
    if (HasCapture())
        return;

    wxAuiToolBarItem* hitItem;
    hitItem = FindToolByPosition(evt.GetX(), evt.GetY());

    if (m_actionItem && hitItem == m_actionItem)
    {
        wxAuiToolBarEvent e(wxEVT_AUITOOLBAR_RIGHT_CLICK, m_actionItem->m_toolId);
        e.SetEventObject(this);
        e.SetToolId(m_actionItem->m_toolId);
        e.SetClickPoint(m_actionPos);
        GetEventHandler()->ProcessEvent(e);
        DoIdleUpdate();
    }
    else
    {
        // right-clicked on the invalid area of the toolbar
        wxAuiToolBarEvent e(wxEVT_AUITOOLBAR_RIGHT_CLICK, -1);
        e.SetEventObject(this);
        e.SetToolId(-1);
        e.SetClickPoint(m_actionPos);
        GetEventHandler()->ProcessEvent(e);
        DoIdleUpdate();
    }

    // reset member variables
    m_actionPos = wxPoint(-1,-1);
    m_actionItem = nullptr;
}

void wxAuiToolBar::OnMiddleDown(wxMouseEvent& evt)
{
    if (HasCapture())
        return;

    wxRect cli_rect(wxPoint(0,0), GetClientSize());

    if (m_gripperSizerItem)
    {
        wxRect gripper_rect = m_gripperSizerItem->GetRect();
        if (gripper_rect.Contains(evt.GetX(), evt.GetY()))
            return;
    }

    if (m_overflowSizerItem && m_art)
    {
        int overflowSize = m_art->GetElementSizeForWindow(wxAUI_TBART_OVERFLOW_SIZE, this);
        if (overflowSize > 0 &&
            evt.m_x > cli_rect.width - overflowSize &&
            evt.m_y >= 0 &&
            evt.m_y < cli_rect.height)
        {
            return;
        }
    }

    m_actionPos = wxPoint(evt.GetX(), evt.GetY());
    m_actionItem = FindToolByPosition(evt.GetX(), evt.GetY());

    if (m_actionItem)
    {
        if (m_actionItem->m_state & wxAUI_BUTTON_STATE_DISABLED)
        {
            m_actionPos = wxPoint(-1,-1);
            m_actionItem = nullptr;
            return;
        }
    }

    UnsetToolTip();
}

void wxAuiToolBar::OnMiddleUp(wxMouseEvent& evt)
{
    if (HasCapture())
        return;

    wxAuiToolBarItem* hitItem;
    hitItem = FindToolByPosition(evt.GetX(), evt.GetY());

    if (m_actionItem && hitItem == m_actionItem)
    {
        if (hitItem->m_kind == wxITEM_NORMAL)
        {
            wxAuiToolBarEvent e(wxEVT_AUITOOLBAR_MIDDLE_CLICK, m_actionItem->m_toolId);
            e.SetEventObject(this);
            e.SetToolId(m_actionItem->m_toolId);
            e.SetClickPoint(m_actionPos);
            GetEventHandler()->ProcessEvent(e);
            DoIdleUpdate();
        }
    }

    // reset member variables
    m_actionPos = wxPoint(-1,-1);
    m_actionItem = nullptr;
}

void wxAuiToolBar::OnMotion(wxMouseEvent& evt)
{
    const bool button_pressed = HasCapture();

    // start a drag event
    if (!m_dragging && button_pressed && m_actionItem &&
        abs(evt.GetX() - m_actionPos.x) + abs(evt.GetY() - m_actionPos.y) > 5)
    {
        // TODO: sending this event only makes sense if there is an 'END_DRAG'
        // event sent sometime in the future (see OnLeftUp())
        wxAuiToolBarEvent e(wxEVT_AUITOOLBAR_BEGIN_DRAG, GetId());
        e.SetEventObject(this);
        e.SetToolId(m_actionItem->m_toolId);
        m_dragging = GetEventHandler()->ProcessEvent(e) && !e.GetSkipped();

        DoIdleUpdate();
    }

    if(m_dragging)
        return;

    wxAuiToolBarItem* hitItem = FindToolByPosition(evt.GetX(), evt.GetY());
    if(button_pressed)
    {
        // if we have a button pressed we want it to be shown in 'depressed'
        // state unless we move the mouse outside the button, then we want it
        // to show as just 'highlighted'
        if (hitItem == m_actionItem)
            SetPressedItem(m_actionItem);
        else
        {
            SetPressedItem(nullptr);
            SetHoverItem(m_actionItem);
        }
    }
    else
    {
        SetHoverItem(hitItem);

        // tooltips handling
        if ( !HasFlag(wxAUI_TB_NO_TOOLTIPS) )
        {
            wxAuiToolBarItem* packingHitItem;
            packingHitItem = FindToolByPositionWithPacking(evt.GetX(), evt.GetY());
            if ( packingHitItem )
            {
                if (packingHitItem != m_tipItem)
                {
                    m_tipItem = packingHitItem;

                    if ( !packingHitItem->m_shortHelp.empty() )
                        SetToolTip(packingHitItem->m_shortHelp);
                    else
                        UnsetToolTip();
                }
            }
            else
            {
                UnsetToolTip();
                m_tipItem = nullptr;
            }
        }

        // figure out the dropdown button state (are we hovering or pressing it?)
        RefreshOverflowState();
    }
}

void wxAuiToolBar::DoResetMouseState()
{
    RefreshOverflowState();
    SetHoverItem(nullptr);
    SetPressedItem(nullptr);

    m_tipItem = nullptr;

    // we have to reset those here, because the mouse-up handlers which do
    // it usually won't be called if we let go of a mouse button while we
    // are outside of the window
    m_actionPos = wxPoint(-1,-1);
    m_actionItem = nullptr;
}

void wxAuiToolBar::OnLeaveWindow(wxMouseEvent& evt)
{
    if(HasCapture())
    {
        evt.Skip();
        return;
    }

    DoResetMouseState();
}

void wxAuiToolBar::OnCaptureLost(wxMouseCaptureLostEvent& WXUNUSED(evt))
{
    m_dragging = false;

    DoResetMouseState();
}

void wxAuiToolBar::OnSetCursor(wxSetCursorEvent& evt)
{
    wxCursor cursor = wxNullCursor;

    if (m_gripperSizerItem)
    {
        wxRect gripper_rect = m_gripperSizerItem->GetRect();
        if (gripper_rect.Contains(evt.GetX(), evt.GetY()))
        {
            cursor = wxCursor(wxCURSOR_SIZING);
        }
    }

    evt.SetCursor(cursor);
}


#endif // wxUSE_AUI

