#ifndef ABOUTAPPINFO_H
#define ABOUTAPPINFO_H

#include <wx/aboutdlg.h>
#include <wx/bitmap.h>
#include <wx/dynarray.h>

class Contributor;

WX_DECLARE_OBJARRAY( Contributor, Contributors );


/**
 * An object of this class is meant to be used to store application specific information
 * like who has contributed in which area of the application, the license, copyright
 * and other descriptive information.
 */
class AboutAppInfo
{
public:
    AboutAppInfo() {};
    virtual ~AboutAppInfo() {};

    void AddDeveloper( const Contributor* developer )   { if( developer  != NULL )
                                                              developers.Add( developer );}
    void AddDocWriter( const Contributor* docwriter )   { if( docwriter  != NULL )
                                                              docwriters.Add( docwriter );}
    void AddArtist( const Contributor* artist )         { if( artist     != NULL )
                                                              artists.Add( artist );}
    void AddTranslator( const Contributor* translator ) { if( translator != NULL )
                                                              translators.Add( translator );}

    Contributors GetDevelopers()  { return developers; }
    Contributors GetDocWriters()  { return docwriters; }
    Contributors GetArtists()     { return artists; }
    Contributors GetTranslators() { return translators; }

    void SetDescription( const wxString& text ) { description = text; }
    wxString& GetDescription() { return description; }

    void SetLicense( const wxString& text ) { license = text; }
    wxString& GetLicense() { return license; }

    void SetCopyright( const wxString& text ) { copyright = text; }
    wxString GetCopyright()
    {
        wxString       copyrightText = copyright;

#if wxUSE_UNICODE
        const wxString utf8_copyrightSign = wxString::FromUTF8( "\xc2\xa9" );
        copyrightText.Replace( _T( "(c)" ), utf8_copyrightSign );
        copyrightText.Replace( _T( "(C)" ), utf8_copyrightSign );
#endif // wxUSE_UNICODE

        return copyrightText;
    }


    void SetAppName( const wxString& name ) { appName = name; }
    wxString& GetAppName() { return appName; }

    void SetBuildVersion( const wxString& version ) { buildVersion = version; }
    wxString& GetBuildVersion() { return buildVersion; }

    void SetLibVersion( const wxString& version ) { libVersion = version; }
    wxString& GetLibVersion() { return libVersion; }

    void SetIcon( const wxIcon& icon ) { appIcon = icon; }
    wxIcon& GetIcon() { return appIcon; }

protected:
private:
    Contributors developers;
    Contributors docwriters;
    Contributors artists;
    Contributors translators;

    wxString     description;
    wxString     license;

    wxString     copyright; // Todo: copyright sign in unicode
    wxString     appName;
    wxString     buildVersion;
    wxString     libVersion;

    wxIcon       appIcon;
};

/**
 * A contributor, a person which was involved in the development of the application
 * or which has contributed in any kind somehow to the project.
 *
 * A contributor consists of the following mandatory information:
 * - Name
 * - EMail address
 *
 * Each contributor can have optional information assigned like:
 * - A category
 * - A category specific icon
 */
class Contributor
{
public:
    Contributor( const wxString& name,
                 const wxString& email,
                 const wxString& category = wxEmptyString,
                 wxBitmap*       icon = NULL ) :
        m_checked( false )
    { m_name = name; m_email = email; m_category = category; m_icon = icon; }
    virtual ~Contributor() {}

    wxString& GetName()     { return m_name; }
    wxString& GetEMail()    { return m_email; }
    wxString& GetCategory() { return m_category; }
    wxBitmap* GetIcon()     { return m_icon; }
    void SetChecked( bool status ) { m_checked = status; }
    bool IsChecked() { return m_checked; }
protected:
private:
    wxString  m_name;
    wxString  m_email;
    wxString  m_category;
    wxBitmap* m_icon;
    bool      m_checked;
};

#endif // ABOUTAPPINFO_H
