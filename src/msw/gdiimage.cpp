///////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/gdiimage.cpp
// Purpose:     wxGDIImage implementation
// Author:      Vadim Zeitlin
// Created:     20.11.99
// Copyright:   (c) 1999 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"


#ifndef WX_PRECOMP
    #include "wx/string.h"
    #include "wx/log.h"
    #include "wx/app.h"
    #include "wx/bitmap.h"
#endif // WX_PRECOMP

#include "wx/msw/private.h"

#include "wx/msw/gdiimage.h"

#if wxUSE_WXDIB
#include "wx/msw/dib.h"
#endif

// By default, use PNG resource handler if we can, i.e. have support for
// loading PNG images in the library. This symbol could be predefined as 0 to
// avoid doing this if anybody ever needs to do it for some reason.
#if !defined(wxUSE_PNG_RESOURCE_HANDLER)
    #define wxUSE_PNG_RESOURCE_HANDLER wxUSE_LIBPNG && wxUSE_IMAGE
#endif

#if wxUSE_PNG_RESOURCE_HANDLER
    #include "wx/image.h"
    #include "wx/utils.h"       // For wxLoadUserResource()
#endif

#include "wx/file.h"

#include "wx/listimpl.cpp"
WX_DEFINE_LIST(wxGDIImageHandlerList)

// ----------------------------------------------------------------------------
// private classes
// ----------------------------------------------------------------------------

// all image handlers are declared/defined in this file because the outside
// world doesn't have to know about them (but only about wxBITMAP_TYPE_XXX ids)

class WXDLLEXPORT wxBMPFileHandler : public wxBitmapHandler
{
public:
    wxBMPFileHandler() : wxBitmapHandler(wxT("Windows bitmap file"), wxT("bmp"),
                                         wxBITMAP_TYPE_BMP)
    {
    }

    virtual bool LoadFile(wxBitmap *bitmap,
                          const wxString& name, wxBitmapType flags,
                          int desiredWidth, int desiredHeight) override;
    virtual bool SaveFile(const wxBitmap *bitmap,
                          const wxString& name, wxBitmapType type,
                          const wxPalette *palette = nullptr) const override;

private:
    wxDECLARE_DYNAMIC_CLASS(wxBMPFileHandler);
};

class WXDLLEXPORT wxBMPResourceHandler: public wxBitmapHandler
{
public:
    wxBMPResourceHandler() : wxBitmapHandler(wxT("Windows bitmap resource"),
                                             wxEmptyString,
                                             wxBITMAP_TYPE_BMP_RESOURCE)
    {
    }

    virtual bool LoadFile(wxBitmap *bitmap,
                          const wxString& name, wxBitmapType flags,
                          int desiredWidth, int desiredHeight) override;

private:
    wxDECLARE_DYNAMIC_CLASS(wxBMPResourceHandler);
};

class WXDLLEXPORT wxIconHandler : public wxGDIImageHandler
{
public:
    wxIconHandler(const wxString& name, const wxString& ext, wxBitmapType type)
        : wxGDIImageHandler(name, ext, type)
    {
    }

    // creating and saving icons is not supported
    virtual bool Create(wxGDIImage *WXUNUSED(image),
                        const void* WXUNUSED(data),
                        wxBitmapType WXUNUSED(flags),
                        int WXUNUSED(width),
                        int WXUNUSED(height),
                        int WXUNUSED(depth) = 1) override
    {
        return false;
    }

    virtual bool Save(const wxGDIImage *WXUNUSED(image),
                      const wxString& WXUNUSED(name),
                      wxBitmapType WXUNUSED(type)) const override
    {
        return false;
    }

    virtual bool Load(wxGDIImage *image,
                      const wxString& name,
                      wxBitmapType flags,
                      int desiredWidth, int desiredHeight) override
    {
        wxIcon *icon = wxDynamicCast(image, wxIcon);
        wxCHECK_MSG( icon, false, wxT("wxIconHandler only works with icons") );

        return LoadIcon(icon, name, flags, desiredWidth, desiredHeight);
    }

protected:
    virtual bool LoadIcon(wxIcon *icon,
                          const wxString& name, wxBitmapType flags,
                          int desiredWidth = -1, int desiredHeight = -1) = 0;
};

class WXDLLEXPORT wxICOFileHandler : public wxIconHandler
{
public:
    wxICOFileHandler() : wxIconHandler(wxT("ICO icon file"),
                                       wxT("ico"),
                                       wxBITMAP_TYPE_ICO)
    {
    }

protected:
    virtual bool LoadIcon(wxIcon *icon,
                          const wxString& name, wxBitmapType flags,
                          int desiredWidth = -1, int desiredHeight = -1) override;

private:
    wxDECLARE_DYNAMIC_CLASS(wxICOFileHandler);
};

class WXDLLEXPORT wxICOResourceHandler: public wxIconHandler
{
public:
    wxICOResourceHandler() : wxIconHandler(wxT("ICO resource"),
                                           wxT("ico"),
                                           wxBITMAP_TYPE_ICO_RESOURCE)
    {
    }

protected:
    virtual bool LoadIcon(wxIcon *icon,
                          const wxString& name, wxBitmapType flags,
                          int desiredWidth = -1, int desiredHeight = -1) override;

private:
    wxDECLARE_DYNAMIC_CLASS(wxICOResourceHandler);
};

#if wxUSE_PNG_RESOURCE_HANDLER

class WXDLLEXPORT wxPNGResourceHandler : public wxBitmapHandler
{
public:
    wxPNGResourceHandler() : wxBitmapHandler(wxS("Windows PNG resource"),
                                             wxString(),
                                             wxBITMAP_TYPE_PNG_RESOURCE)
    {
    }

    virtual bool LoadFile(wxBitmap *bitmap,
                          const wxString& name, wxBitmapType flags,
                          int desiredWidth, int desiredHeight) override;

private:
    wxDECLARE_DYNAMIC_CLASS(wxPNGResourceHandler);
};

#endif // wxUSE_PNG_RESOURCE_HANDLER

// ----------------------------------------------------------------------------
// wxWin macros
// ----------------------------------------------------------------------------

wxIMPLEMENT_DYNAMIC_CLASS(wxBMPFileHandler, wxBitmapHandler);
wxIMPLEMENT_DYNAMIC_CLASS(wxBMPResourceHandler, wxBitmapHandler);
wxIMPLEMENT_DYNAMIC_CLASS(wxICOFileHandler, wxObject);
wxIMPLEMENT_DYNAMIC_CLASS(wxICOResourceHandler, wxObject);
#if wxUSE_PNG_RESOURCE_HANDLER
wxIMPLEMENT_DYNAMIC_CLASS(wxPNGResourceHandler, wxBitmapHandler);
#endif // wxUSE_PNG_RESOURCE_HANDLER

// ----------------------------------------------------------------------------
// private functions
// ----------------------------------------------------------------------------


// ============================================================================
// implementation
// ============================================================================

wxGDIImageHandlerList wxGDIImage::ms_handlers;

// ----------------------------------------------------------------------------
// wxGDIImage functions forwarded to wxGDIImageRefData
// ----------------------------------------------------------------------------

bool wxGDIImage::FreeResource(bool WXUNUSED(force))
{
    if ( !IsNull() )
    {
        GetGDIImageData()->Free();
        GetGDIImageData()->m_handle = 0;
    }

    return true;
}

WXHANDLE wxGDIImage::GetResourceHandle() const
{
    return GetHandle();
}

// ----------------------------------------------------------------------------
// wxGDIImage handler stuff
// ----------------------------------------------------------------------------

void wxGDIImage::AddHandler(wxGDIImageHandler *handler)
{
    ms_handlers.Append(handler);
}

void wxGDIImage::InsertHandler(wxGDIImageHandler *handler)
{
    ms_handlers.Insert(handler);
}

bool wxGDIImage::RemoveHandler(const wxString& name)
{
    wxGDIImageHandler *handler = FindHandler(name);
    if ( handler )
    {
        ms_handlers.DeleteObject(handler);
        return true;
    }
    else
        return false;
}

wxGDIImageHandler *wxGDIImage::FindHandler(const wxString& name)
{
    wxGDIImageHandlerList::compatibility_iterator node = ms_handlers.GetFirst();
    while ( node )
    {
        wxGDIImageHandler *handler = node->GetData();
        if ( handler->GetName() == name )
            return handler;
        node = node->GetNext();
    }

    return nullptr;
}

wxGDIImageHandler *wxGDIImage::FindHandler(const wxString& extension,
                                           long type)
{
    wxGDIImageHandlerList::compatibility_iterator node = ms_handlers.GetFirst();
    while ( node )
    {
        wxGDIImageHandler *handler = node->GetData();
        if ( (handler->GetExtension() == extension) &&
             (type == -1 || handler->GetType() == type) )
        {
            return handler;
        }

        node = node->GetNext();
    }
    return nullptr;
}

wxGDIImageHandler *wxGDIImage::FindHandler(long type)
{
    wxGDIImageHandlerList::compatibility_iterator node = ms_handlers.GetFirst();
    while ( node )
    {
        wxGDIImageHandler *handler = node->GetData();
        if ( handler->GetType() == type )
            return handler;

        node = node->GetNext();
    }

    return nullptr;
}

void wxGDIImage::CleanUpHandlers()
{
    wxGDIImageHandlerList::compatibility_iterator node = ms_handlers.GetFirst();
    while ( node )
    {
        wxGDIImageHandler *handler = node->GetData();
        wxGDIImageHandlerList::compatibility_iterator next = node->GetNext();
        delete handler;
        ms_handlers.Erase( node );
        node = next;
    }
}

void wxGDIImage::InitStandardHandlers()
{
    AddHandler(new wxBMPResourceHandler);
    AddHandler(new wxBMPFileHandler);
    AddHandler(new wxICOFileHandler);
    AddHandler(new wxICOResourceHandler);
#if wxUSE_PNG_RESOURCE_HANDLER
    AddHandler(new wxPNGResourceHandler);
#endif // wxUSE_PNG_RESOURCE_HANDLER
}

// ----------------------------------------------------------------------------
// scale factor-related functions
// ----------------------------------------------------------------------------

// wxMSW doesn't really use scale factor, but we must still store it to use the
// correct sizes in the code which uses it to decide on the bitmap size to use.

void wxGDIImage::SetScaleFactor(double scale)
{
    wxCHECK_RET( IsOk(), wxT("invalid bitmap") );

    if ( GetGDIImageData()->m_scaleFactor != scale )
    {
        AllocExclusive();

        // Note that GetGDIImageData() result may have changed after calling
        // AllocExclusive(), so don't be tempted to optimize it.
        GetGDIImageData()->m_scaleFactor = scale;
    }
}

double wxGDIImage::GetScaleFactor() const
{
    wxCHECK_MSG( IsOk(), -1, wxT("invalid bitmap") );

    return GetGDIImageData()->m_scaleFactor;
}

wxSize wxGDIImage::GetDIPSize() const
{
    return GetSize() / GetScaleFactor();
}

double wxGDIImage::GetLogicalWidth() const
{
    return GetWidth();
}

double wxGDIImage::GetLogicalHeight() const
{
    return GetHeight();
}

wxSize wxGDIImage::GetLogicalSize() const
{
    return GetSize();
}

// ----------------------------------------------------------------------------
// wxBitmap handlers
// ----------------------------------------------------------------------------

bool wxBMPResourceHandler::LoadFile(wxBitmap *bitmap,
                                    const wxString& name, wxBitmapType WXUNUSED(flags),
                                    int WXUNUSED(desiredWidth),
                                    int WXUNUSED(desiredHeight))
{
    // TODO: load colourmap.
    HBITMAP hbmp = ::LoadBitmap(wxGetInstance(), name.t_str());
    if ( hbmp == nullptr )
    {
        // it's probably not found
        wxLogError(wxT("Can't load bitmap '%s' from resources! Check .rc file."),
            name.c_str());

        return false;
    }

    int w, h, d;
    BITMAP bm;
    if (::GetObject(hbmp, sizeof(BITMAP), &bm))
    {
        w = bm.bmWidth;
        h = bm.bmHeight;
        d = bm.bmBitsPixel;
    }
    else
    {
        wxLogLastError(wxT("GetObject(HBITMAP)"));
        w = h = d = 0;
    }

    bitmap->InitFromHBITMAP((WXHBITMAP)hbmp, w, h, d);

    // use 0xc0c0c0 as transparent colour by default
    bitmap->SetMask(new wxMask(*bitmap, *wxLIGHT_GREY));

    return true;
}

bool wxBMPFileHandler::LoadFile(wxBitmap *bitmap,
                                const wxString& name, wxBitmapType WXUNUSED(flags),
                                int WXUNUSED(desiredWidth),
                                int WXUNUSED(desiredHeight))
{
    wxCHECK_MSG( bitmap, false, wxT("null bitmap in LoadFile") );

#if wxUSE_WXDIB
    // Try loading using native Windows LoadImage() first.
    wxDIB dib(name);
    if ( dib.IsOk() )
        return bitmap->CopyFromDIB(dib);
#endif // wxUSE_WXDIB

    // Some valid bitmap files are not supported by LoadImage(), e.g. those
    // with negative height. Try to use our own bitmap loading code which does
    // support them.
#if wxUSE_IMAGE
    wxImage img(name, wxBITMAP_TYPE_BMP);
    if ( img.IsOk() )
    {
        *bitmap = wxBitmap(img);
        return true;
    }
#endif // wxUSE_IMAGE

    return false;
}

bool wxBMPFileHandler::SaveFile(const wxBitmap *bitmap,
                                const wxString& name,
                                wxBitmapType WXUNUSED(type),
                                const wxPalette * WXUNUSED(pal)) const
{
#if wxUSE_WXDIB
    wxCHECK_MSG( bitmap, false, wxT("null bitmap in SaveFile") );

    wxDIB dib(*bitmap);

    return dib.Save(name);
#else
    return false;
#endif
}

// ----------------------------------------------------------------------------
// wxIcon handlers
// ----------------------------------------------------------------------------

bool wxICOFileHandler::LoadIcon(wxIcon *icon,
                                const wxString& name,
                                wxBitmapType WXUNUSED(flags),
                                int desiredWidth, int desiredHeight)
{
    icon->UnRef();

    HICON hicon = nullptr;

    // Parse the filename: it may be of the form "filename;n" in order to
    // specify the nth icon in the file.
    //
    // For the moment, ignore the issue of possible semicolons in the
    // filename.
    int iconIndex = 0;
    wxString nameReal(name);
    wxString strIconIndex = name.AfterLast(wxT(';'));
    if (strIconIndex != name)
    {
        iconIndex = wxAtoi(strIconIndex);
        nameReal = name.BeforeLast(wxT(';'));
    }

#if 0
    // If we don't know what size icon we're looking for,
    // try to find out what's there.
    // Unfortunately this doesn't work, because ExtractIconEx
    // will scale the icon to the 'desired' size, even if that
    // size of icon isn't explicitly stored. So we would have
    // to parse the icon file ourselves.
    if ( desiredWidth == -1 &&
         desiredHeight == -1)
    {
        // Try loading a large icon first
        if ( ::ExtractIconEx(nameReal, iconIndex, &hicon, nullptr, 1) == 1)
        {
        }
        // Then try loading a small icon
        else if ( ::ExtractIconEx(nameReal, iconIndex, nullptr, &hicon, 1) == 1)
        {
        }
    }
    else
#endif
    // were we asked for a large icon?
    const wxWindow* win = wxApp::GetMainTopWindow();
    if ( desiredWidth == wxGetSystemMetrics(SM_CXICON, win) &&
         desiredHeight == wxGetSystemMetrics(SM_CYICON, win) )
    {
        // get the specified large icon from file
        if ( !::ExtractIconEx(nameReal.t_str(), iconIndex, &hicon, nullptr, 1) )
        {
            // it is not an error, but it might still be useful to be informed
            // about it optionally
            wxLogTrace(wxT("iconload"),
                       wxT("No large icons found in the file '%s'."),
                       name.c_str());
        }
    }
    else if ( desiredWidth == wxGetSystemMetrics(SM_CXSMICON, win) &&
              desiredHeight == wxGetSystemMetrics(SM_CYSMICON, win) )
    {
        // get the specified small icon from file
        if ( !::ExtractIconEx(nameReal.t_str(), iconIndex, nullptr, &hicon, 1) )
        {
            wxLogTrace(wxT("iconload"),
                       wxT("No small icons found in the file '%s'."),
                       name.c_str());
        }
    }
    //else: not standard size, load below

    if ( !hicon )
    {
        // take any size icon from the file by index
        hicon = ::ExtractIcon(wxGetInstance(), nameReal.t_str(), iconIndex);
    }

    if ( !hicon )
    {
        wxLogSysError(wxT("Failed to load icon from the file '%s'"),
                      name.c_str());

        return false;
    }

    if ( !icon->CreateFromHICON(hicon) )
        return false;

    if ( (desiredWidth != -1 && desiredWidth != icon->GetWidth()) ||
         (desiredHeight != -1 && desiredHeight != icon->GetHeight()) )
    {
        wxLogTrace(wxT("iconload"),
                   wxT("Returning false from wxICOFileHandler::Load because of the size mismatch: actual (%d, %d), requested (%d, %d)"),
                   icon->GetWidth(), icon->GetHeight(),
                   desiredWidth, desiredHeight);

        icon->UnRef();

        return false;
    }

    return true;
}

bool wxICOResourceHandler::LoadIcon(wxIcon *icon,
                                    const wxString& name,
                                    wxBitmapType WXUNUSED(flags),
                                    int desiredWidth, int desiredHeight)
{
    HICON hicon;

    // do we need the icon of the specific size or would any icon do?
    bool hasSize = desiredWidth != -1 || desiredHeight != -1;

    wxASSERT_MSG( !hasSize || (desiredWidth != -1 && desiredHeight != -1),
                  wxT("width and height should be either both -1 or not") );

    // try to load the icon from this program first to allow overriding the
    // standard icons (although why one would want to do it considering that
    // we already have wxApp::GetStdIcon() is unclear)

    // note that we can't just always call LoadImage() because it seems to do
    // some icon rescaling internally which results in very ugly 16x16 icons
    if ( hasSize )
    {
        hicon = (HICON)::LoadImage(wxGetInstance(), name.t_str(), IMAGE_ICON,
                                    desiredWidth, desiredHeight,
                                    LR_DEFAULTCOLOR);
        if ( !hicon )
        {
            wxLogLastError(wxString::Format("LoadImage(%s)", name));
        }
    }
    else
    {
        hicon = ::LoadIcon(wxGetInstance(), name.t_str());
        if ( !hicon )
        {
            wxLogLastError(wxString::Format("LoadIcon(%s)", name));
        }
    }

    // next check if it's not a standard icon
    if ( !hicon && !hasSize )
    {
        static const struct
        {
            const wxChar *name;
            LPTSTR id;
        } stdIcons[] =
        {
            { wxT("wxICON_QUESTION"),   IDI_QUESTION    },
            { wxT("wxICON_WARNING"),    IDI_EXCLAMATION },
            { wxT("wxICON_ERROR"),      IDI_HAND        },
            { wxT("wxICON_INFORMATION"),       IDI_ASTERISK    },
        };

        for ( size_t nIcon = 0; !hicon && nIcon < WXSIZEOF(stdIcons); nIcon++ )
        {
            if ( name == stdIcons[nIcon].name )
            {
                hicon = ::LoadIcon((HINSTANCE)nullptr, stdIcons[nIcon].id);
                if ( !hicon )
                {
                    wxLogLastError(wxString::Format("LoadIcon(%s)", stdIcons[nIcon].name));
                }
                break;
            }
        }
    }

    if ( !hicon )
        return false;

    wxSize size;
    double scale = 1.0;
    if ( hasSize )
    {
        size.x = desiredWidth;
        size.y = desiredHeight;
    }
    else // We loaded an icon of default size.
    {
        // LoadIcon() returns icons of scaled size, so we must use the correct
        // scaling factor of them.
        size = wxGetHiconSize(hicon);
        if ( const wxWindow* win = wxApp::GetMainTopWindow() )
            scale = win->GetDPIScaleFactor();
    }

    return icon->InitFromHICON((WXHICON)hicon, size.x, size.y, scale);
}

#if wxUSE_PNG_RESOURCE_HANDLER

// ----------------------------------------------------------------------------
// PNG handler
// ----------------------------------------------------------------------------

bool wxPNGResourceHandler::LoadFile(wxBitmap *bitmap,
                                    const wxString& name,
                                    wxBitmapType WXUNUSED(flags),
                                    int WXUNUSED(desiredWidth),
                                    int WXUNUSED(desiredHeight))
{
    const void* pngData = nullptr;
    size_t pngSize = 0;

    // Currently we hardcode RCDATA resource type as this is what is usually
    // used for the embedded images. We could allow specifying the type as part
    // of the name in the future (e.g. "type:name" or something like this) if
    // really needed.
    if ( !wxLoadUserResource(&pngData, &pngSize,
                             name,
                             RT_RCDATA,
                             wxGetInstance()) )
    {
        // Notice that this message is not translated because only the
        // programmer (and not the end user) can make any use of it.
        wxLogError(wxS("Bitmap in PNG format \"%s\" not found, check ")
                   wxS("that the resource file contains \"RCDATA\" ")
                   wxS("resource with this name."),
                   name);

        return false;
    }

    *bitmap = wxBitmap::NewFromPNGData(pngData, pngSize);
    if ( !bitmap->IsOk() )
    {
        wxLogError(wxS("Couldn't load resource bitmap \"%s\" as a PNG. ")
                   wxS("Have you registered PNG image handler?"),
                   name);

        return false;
    }

    return true;
}

#endif // wxUSE_PNG_RESOURCE_HANDLER

// ----------------------------------------------------------------------------
// private functions
// ----------------------------------------------------------------------------

wxSize wxGetHiconSize(HICON hicon)
{
    wxSize size;

    if ( hicon )
    {
        AutoIconInfo info;
        if ( info.GetFrom(hicon) )
        {
            HBITMAP hbmp = info.hbmMask;
            if ( hbmp )
            {
                BITMAP bm;
                if ( ::GetObject(hbmp, sizeof(BITMAP), (LPSTR) &bm) )
                {
                    size = wxSize(bm.bmWidth, bm.bmHeight);
                }
            }
            // For monochrome icon reported height is doubled
            // because it contains both AND and XOR masks.
            if ( info.hbmColor == nullptr )
            {
                size.y /= 2;
            }
        }
    }

    if ( !size.x )
    {
        // use default icon size on this hardware
        const wxWindow* win = wxApp::GetMainTopWindow();
        size.x = wxGetSystemMetrics(SM_CXICON, win);
        size.y = wxGetSystemMetrics(SM_CYICON, win);
    }

    return size;
}
