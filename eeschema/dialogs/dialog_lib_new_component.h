/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2009-2105 Wayne Stambaugh <stambaughw@verizon.net>
 * Copyright (C) 2015-2019 KiCad Developers, see CHANGELOG.TXT for contributors.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you may find one here:
 * http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
 * or you may search the http://www.gnu.org website for the version 2 license,
 * or you may write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#ifndef __dialog_lib_new_component__
#define __dialog_lib_new_component__

/**
 * @file
 * Subclass of DIALOG_LIB_NEW_COMPONENT, which is generated by wxFormBuilder.
 */

#include <dialog_lib_new_component_base.h>

/** Implementing DIALOG_LIB_NEW_COMPONENT */
class DIALOG_LIB_NEW_COMPONENT : public DIALOG_LIB_NEW_COMPONENT_BASE
{
public:
    /** Constructor */
    DIALOG_LIB_NEW_COMPONENT( wxWindow* parent, const wxArrayString* aRootSymbolNames = nullptr );

    void SetName( const wxString& name ) override { m_textName->SetValue( name ); }
    wxString GetName( void ) const override { return m_textName->GetValue(); }

    wxString GetParentSymbolName() const { return m_comboInheritanceSelect->GetValue(); }

    void SetReference( const wxString& reference )
    {
        m_textReference->SetValue( reference );
    }
    wxString GetReference( void ) { return m_textReference->GetValue(); }

    void SetPartCount( int count ) { m_spinPartCount->SetValue( count ); }
    int GetUnitCount( void ) { return m_spinPartCount->GetValue(); }

    void SetAlternateBodyStyle( bool enable )
    {
        m_checkHasConversion->SetValue( enable );
    }
    bool GetAlternateBodyStyle( void )
    {
        return m_checkHasConversion->GetValue();
    }

    void SetPowerSymbol( bool enable )
    {
        m_checkIsPowerSymbol->SetValue( enable );
    }
    bool GetPowerSymbol( void ) { return m_checkIsPowerSymbol->GetValue(); }

    void SetLockItems( bool enable ) { m_checkLockItems->SetValue( enable ); }
    bool GetLockItems( void ) { return m_checkLockItems->GetValue(); }

    void SetPinTextPosition( int position )
    {
        m_spinPinTextPosition->SetValue( position );
    }
    int GetPinTextPosition( void ) { return m_spinPinTextPosition->GetValue(); }

    void SetShowPinNumber( bool show )
    {
        m_checkShowPinNumber->SetValue( show );
    }
    bool GetShowPinNumber( void ) { return m_checkShowPinNumber->GetValue(); }

    void SetShowPinName( bool show )
    {
        m_checkShowPinName->SetValue( show );
    }
    bool GetShowPinName( void ) { return m_checkShowPinName->GetValue(); }

    void SetPinNameInside( bool show )
    {
        m_checkShowPinNameInside->SetValue( show );
    }
    bool GetPinNameInside( void ) { return m_checkShowPinNameInside->GetValue(); }

protected:
    virtual void OnParentSymbolSelect( wxCommandEvent& event ) override;

private:
    void syncControls( bool aIsDerivedPart );
};

#endif // __dialog_lib_new_component__
