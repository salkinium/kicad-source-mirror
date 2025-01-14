/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2019  KiCad Developers, see CHANGELOG.TXT for contributors.
 * Copyright (C) 2016-2017 CERN
 * @author Maciej Suminski <maciej.suminski@cern.ch>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 3
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you may find one here:
 * https://www.gnu.org/licenses/gpl-3.0.html
 * or you may search the http://www.gnu.org website for the version 3 license,
 * or you may write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#include "wildcards_and_files_ext.h"
#include "dialog_spice_model.h"

#include <sim/spice_value.h>
#include <confirm.h>
#include <project.h>

#include <wx/tokenzr.h>
#include <wx/wupdlock.h>

#include <cctype>

// Helper function to shorten conditions
static bool empty( const wxTextCtrl* aCtrl )
{
    return aCtrl->GetValue().IsEmpty();
}


// Function to sort PWL values list
static int wxCALLBACK comparePwlValues( wxIntPtr aItem1, wxIntPtr aItem2, wxIntPtr WXUNUSED( aSortData ) )
{
    float* t1 = reinterpret_cast<float*>( &aItem1 );
    float* t2 = reinterpret_cast<float*>( &aItem2 );

    if( *t1 > *t2 )
        return 1;

    if( *t1 < *t2 )
        return -1;

    return 0;
}


// Structure describing a type of Spice model
struct SPICE_MODEL_INFO
{
    SPICE_PRIMITIVE type;               ///< Character identifying the model
    wxString description;               ///< Human-readable description
    std::vector<std::string> keywords;  ///< Keywords indicating the model
};


// Recognized model types
static const std::vector<SPICE_MODEL_INFO> modelTypes =
{
    { SP_DIODE,     _( "Diode" ),      { "d" } },
    { SP_BJT,       _( "BJT" ),        { "npn", "pnp" } },
    { SP_MOSFET,    _( "MOSFET" ),     { "nmos", "pmos", "vdmos" } },
    { SP_JFET,      _( "JFET" ),       { "njf", "pjf" } },
    { SP_SUBCKT,    _( "Subcircuit" ), {} },
};


// Returns index of an entry in modelTypes array (above) corresponding to a Spice primitive
static int getModelTypeIdx( char aPrimitive )
{
    const char prim = std::toupper( aPrimitive );

    for( size_t i = 0; i < modelTypes.size(); ++i )
    {
        if( modelTypes[i].type == prim )
            return i;
    }

    return -1;
}


DIALOG_SPICE_MODEL::DIALOG_SPICE_MODEL( wxWindow* aParent, SCH_COMPONENT& aComponent, SCH_FIELDS* aFields )
    : DIALOG_SPICE_MODEL_BASE( aParent ), m_component( aComponent ), m_schfields( aFields ),
      m_libfields( nullptr ), m_useSchFields( true ),
      m_spiceEmptyValidator( true ), m_notEmptyValidator( wxFILTER_EMPTY )
{
    Init();
}


DIALOG_SPICE_MODEL::DIALOG_SPICE_MODEL( wxWindow* aParent, SCH_COMPONENT& aComponent, LIB_FIELDS* aFields )
    : DIALOG_SPICE_MODEL_BASE( aParent ), m_component( aComponent ), m_schfields( nullptr ),
      m_libfields( aFields ), m_useSchFields( false ),
      m_spiceEmptyValidator( true ), m_notEmptyValidator( wxFILTER_EMPTY )
{
    Init();
}


void DIALOG_SPICE_MODEL::Init()
{
    m_pasValue->SetValidator( m_spiceValidator );

    m_modelType->SetValidator( m_notEmptyValidator );
    m_modelType->Clear();

    // Create a list of handled models
    for( const auto& model : modelTypes )
        m_modelType->Append( model.description );

    m_modelName->SetValidator( m_notEmptyValidator );

    m_genDc->SetValidator( m_spiceEmptyValidator );
    m_genAcMag->SetValidator( m_spiceEmptyValidator );
    m_genAcPhase->SetValidator( m_spiceEmptyValidator );

    m_pulseInit->SetValidator( m_spiceEmptyValidator );
    m_pulseNominal->SetValidator( m_spiceEmptyValidator );
    m_pulseDelay->SetValidator( m_spiceEmptyValidator );
    m_pulseRise->SetValidator( m_spiceEmptyValidator );
    m_pulseFall->SetValidator( m_spiceEmptyValidator );
    m_pulseWidth->SetValidator( m_spiceEmptyValidator );
    m_pulsePeriod->SetValidator( m_spiceEmptyValidator );

    m_sinOffset->SetValidator( m_spiceEmptyValidator );
    m_sinAmplitude->SetValidator( m_spiceEmptyValidator );
    m_sinFreq->SetValidator( m_spiceEmptyValidator );
    m_sinDelay->SetValidator( m_spiceEmptyValidator );
    m_sinDampFactor->SetValidator( m_spiceEmptyValidator );

    m_expInit->SetValidator( m_spiceEmptyValidator );
    m_expPulsed->SetValidator( m_spiceEmptyValidator );
    m_expRiseDelay->SetValidator( m_spiceEmptyValidator );
    m_expRiseConst->SetValidator( m_spiceEmptyValidator );
    m_expFallDelay->SetValidator( m_spiceEmptyValidator );
    m_expFallConst->SetValidator( m_spiceEmptyValidator );

    m_pwlTimeCol = m_pwlValList->AppendColumn( "Time [s]", wxLIST_FORMAT_LEFT, 100 );
    m_pwlValueCol = m_pwlValList->AppendColumn( "Value [V/A]", wxLIST_FORMAT_LEFT, 100 );

    m_sdbSizerOK->SetDefault();
}


bool DIALOG_SPICE_MODEL::TransferDataFromWindow()
{
    if( !DIALOG_SPICE_MODEL_BASE::TransferDataFromWindow() )
        return false;

    wxWindow* page = m_notebook->GetCurrentPage();

    // Passive
    if( page == m_passive )
    {
        if( !m_passive->Validate() )
            return false;

        switch( m_pasType->GetSelection() )
        {
            case 0: m_fieldsTmp[SF_PRIMITIVE] = (char) SP_RESISTOR; break;
            case 1: m_fieldsTmp[SF_PRIMITIVE] = (char) SP_CAPACITOR; break;
            case 2: m_fieldsTmp[SF_PRIMITIVE] = (char) SP_INDUCTOR; break;

            default:
                wxASSERT_MSG( false, "Unhandled passive type" );
                return false;
                break;
        }

        m_fieldsTmp[SF_MODEL] = m_pasValue->GetValue();
    }


    // Model
    else if( page == m_model )
    {
        if( !m_model->Validate() )
            return false;

        int modelIdx = m_modelType->GetSelection();

        if( modelIdx > 0 && modelIdx < (int)modelTypes.size() )
            m_fieldsTmp[SF_PRIMITIVE] = static_cast<char>( modelTypes[modelIdx].type );

        m_fieldsTmp[SF_MODEL] = m_modelName->GetValue();

        if( !empty( m_modelLibrary ) )
            m_fieldsTmp[SF_LIB_FILE] = m_modelLibrary->GetValue();
    }

    // Power source
    else if( page == m_power )
    {
        wxString model;

        if( !generatePowerSource( model ) )
            return false;

        m_fieldsTmp[SF_PRIMITIVE] = (char)( m_pwrType->GetSelection() ? SP_ISOURCE : SP_VSOURCE );
        m_fieldsTmp[SF_MODEL] = model;
    }


    else
    {
        wxASSERT_MSG( false, "Unhandled model type" );
        return false;
    }

    m_fieldsTmp[SF_ENABLED] = !m_disabled->GetValue() ? "Y" : "N";        // note bool inversion
    m_fieldsTmp[SF_NODE_SEQUENCE] = m_nodeSeqCheck->IsChecked() ? m_nodeSeqVal->GetValue() : "";

    // Apply the settings
    for( int i = 0; i < SF_END; ++i )
    {
        if( m_fieldsTmp.count( (SPICE_FIELD) i ) > 0 && !m_fieldsTmp.at( i ).IsEmpty() )
        {
            if( m_useSchFields )
                getSchField( i ).SetText( m_fieldsTmp[i] );
            else
                getLibField( i ).SetText( m_fieldsTmp[i] );
        }
        else
        {
            // Erase empty fields (having empty fields causes a warning in the properties dialog)
            const wxString& spiceField = NETLIST_EXPORTER_PSPICE::GetSpiceFieldName( (SPICE_FIELD) i );

            if( m_useSchFields )
                m_schfields->erase( std::remove_if( m_schfields->begin(), m_schfields->end(),
                        [&]( const SCH_FIELD& f ) { return f.GetName() == spiceField; } ), m_schfields->end() );
            else
                m_libfields->erase( std::remove_if( m_libfields->begin(), m_libfields->end(),
                        [&]( const LIB_FIELD& f ) { return f.GetName() == spiceField; } ), m_libfields->end() );
        }
    }

    return true;
}


bool DIALOG_SPICE_MODEL::TransferDataToWindow()
{
    const auto& spiceFields = NETLIST_EXPORTER_PSPICE::GetSpiceFields();

    // Fill out the working buffer
    for( unsigned int idx = 0; idx < spiceFields.size(); ++idx )
    {
        const wxString& spiceField = spiceFields[idx];

        m_fieldsTmp[idx] = NETLIST_EXPORTER_PSPICE::GetSpiceFieldDefVal( (SPICE_FIELD) idx, &m_component,
            NET_ADJUST_INCLUDE_PATHS | NET_ADJUST_PASSIVE_VALS );

        // Do not modify the existing value, just add missing fields with default values
        if( m_useSchFields && m_schfields )
        {
            for( const auto& field : *m_schfields )
            {
                if( field.GetName() == spiceField  && !field.GetText().IsEmpty() )
                {
                    m_fieldsTmp[idx] = field.GetText();
                    break;
                }
            }
        }
        else if( m_libfields)
        {
            // TODO: There must be a good way to template out these repetitive calls
            for( const auto& field : *m_libfields )
            {
                if( field.GetName() == spiceField  && !field.GetText().IsEmpty() )
                {
                    m_fieldsTmp[idx] = field.GetText();
                    break;
                }
            }
        }
    }

    // Analyze the component fields to fill out the dialog
    char primitive = toupper( m_fieldsTmp[SF_PRIMITIVE][0] );

    switch( primitive )
    {
        case SP_RESISTOR:
        case SP_CAPACITOR:
        case SP_INDUCTOR:
            m_notebook->SetSelection( m_notebook->FindPage( m_passive ) );
            m_pasType->SetSelection( primitive == SP_RESISTOR ? 0
                    : primitive == SP_CAPACITOR ? 1
                    : primitive == SP_INDUCTOR ? 2
                    : -1 );
            m_pasValue->SetValue( m_fieldsTmp[SF_MODEL] );
            break;

        case SP_DIODE:
        case SP_BJT:
        case SP_MOSFET:
        case SP_JFET:
        case SP_SUBCKT:
            m_notebook->SetSelection( m_notebook->FindPage( m_model ) );
            m_modelType->SetSelection( getModelTypeIdx( primitive ) );
            m_modelName->SetValue( m_fieldsTmp[SF_MODEL] );
            m_modelLibrary->SetValue( m_fieldsTmp[SF_LIB_FILE] );

            if( !empty( m_modelLibrary ) )
            {
                const wxString& libFile = m_modelLibrary->GetValue();
                m_fieldsTmp[SF_LIB_FILE] = libFile;
                loadLibrary( libFile );
            }
            break;

        case SP_VSOURCE:
        case SP_ISOURCE:
            if( !parsePowerSource( m_fieldsTmp[SF_MODEL] ) )
                return false;

            m_notebook->SetSelection( m_notebook->FindPage( m_power ) );
            m_pwrType->SetSelection( primitive == SP_ISOURCE ? 1 : 0 );
            break;

        default:
            //wxASSERT_MSG( false, "Unhandled Spice primitive type" );
            break;
    }

    m_disabled->SetValue( !NETLIST_EXPORTER_PSPICE::StringToBool( m_fieldsTmp[SF_ENABLED] ) );

    // Check if node sequence is different than the default one
    if( m_fieldsTmp[SF_NODE_SEQUENCE]
            != NETLIST_EXPORTER_PSPICE::GetSpiceFieldDefVal( SF_NODE_SEQUENCE, &m_component, 0 ) )
    {
        m_nodeSeqCheck->SetValue( true );
        m_nodeSeqVal->SetValue( m_fieldsTmp[SF_NODE_SEQUENCE] );
    }

    return DIALOG_SPICE_MODEL_BASE::TransferDataToWindow();
}


bool DIALOG_SPICE_MODEL::parsePowerSource( const wxString& aModel )
{
    if( aModel.IsEmpty() )
        return false;

    wxStringTokenizer tokenizer( aModel, " ()" );
    wxString tkn = tokenizer.GetNextToken().Lower();

    while( tokenizer.HasMoreTokens() )
    {
        // Variables used for generic values processing (filling out wxTextCtrls in sequence)
        bool genericProcessing = false;
        unsigned int genericReqParamsCount = 0;
        std::vector<wxTextCtrl*> genericControls;

        if( tkn == "dc" )
        {
            // There might be an optional "dc" or "trans" directive, skip it
            if( tkn == "dc" || tkn == "trans" )
                tkn = tokenizer.GetNextToken().Lower();

            // DC value
            try
            {
                m_genDc->SetValue( SPICE_VALUE( tkn ).ToSpiceString() );
            }
            catch( ... )
            {
                return false;
            }
        }


        else if( tkn == "ac" )
        {
            // AC magnitude
            try
            {
                tkn = tokenizer.GetNextToken().Lower();
                m_genAcMag->SetValue( SPICE_VALUE( tkn ).ToSpiceString() );
            }
            catch( ... )
            {
                return false;
            }

            // AC phase (optional)
            try
            {
                tkn = tokenizer.GetNextToken().Lower();
                m_genAcPhase->SetValue( SPICE_VALUE( tkn ).ToSpiceString() );
            }
            catch( ... )
            {
                continue;   // perhaps another directive
            }
        }


        else if( tkn == "pulse" )
        {
            m_powerNotebook->SetSelection( m_powerNotebook->FindPage( m_pwrPulse ) );

            genericProcessing = true;
            genericReqParamsCount = 2;
            genericControls = { m_pulseInit, m_pulseNominal, m_pulseDelay,
                m_pulseRise, m_pulseFall, m_pulseWidth, m_pulsePeriod };
        }


        else if( tkn == "sin" )
        {
            m_powerNotebook->SetSelection( m_powerNotebook->FindPage( m_pwrSin ) );

            genericProcessing = true;
            genericReqParamsCount = 2;
            genericControls = { m_sinOffset, m_sinAmplitude, m_sinFreq, m_sinDelay, m_sinDampFactor };
        }


        else if( tkn == "exp" )
        {
            m_powerNotebook->SetSelection( m_powerNotebook->FindPage( m_pwrExp ) );

            genericProcessing = true;
            genericReqParamsCount = 2;
            genericControls = { m_expInit, m_expPulsed,
                m_expRiseDelay, m_expRiseConst, m_expFallDelay, m_expFallConst };
        }


        else if( tkn == "pwl" )
        {
            m_powerNotebook->SetSelection( m_powerNotebook->FindPage( m_pwrPwl ) );

            try
            {
                while( tokenizer.HasMoreTokens() )
                {
                    tkn = tokenizer.GetNextToken();
                    SPICE_VALUE time( tkn );

                    tkn = tokenizer.GetNextToken();
                    SPICE_VALUE value( tkn );

                    addPwlValue( time.ToSpiceString(), value.ToSpiceString() );
                }
            }
            catch( ... )
            {
                return false;
            }
        }


        else
        {
            // Unhandled power source type
            wxASSERT_MSG( false, "Unhandled power source type" );
            return false;
        }


        if( genericProcessing )
        {
            try
            {
                for( unsigned int i = 0; i < genericControls.size(); ++i )
                {
                    // If there are no more tokens, let's check if we got at least required fields
                    if( !tokenizer.HasMoreTokens() )
                        return ( i >= genericReqParamsCount );

                    tkn = tokenizer.GetNextToken().Lower();
                    genericControls[i]->SetValue( SPICE_VALUE( tkn ).ToSpiceString() );
                }
            }
            catch( ... )
            {
                return false;
            }
        }

        // Get the next token now, so if any of the branches catches an exception, try to
        // process it in another branch
        tkn = tokenizer.GetNextToken().Lower();
    }

    return true;
}


bool DIALOG_SPICE_MODEL::generatePowerSource( wxString& aTarget ) const
{
    wxString acdc, trans;
    wxWindow* page = m_powerNotebook->GetCurrentPage();
    bool useTrans = true;       // shall we use the transient command part?

    // Variables for generic processing
    bool genericProcessing = false;
    unsigned int genericReqParamsCount = 0;
    std::vector<wxTextCtrl*> genericControls;

    /// DC / AC section
    // If SPICE_VALUE can be properly constructed, then it is a valid value
    try
    {
        if( !empty( m_genDc ) )
            acdc += wxString::Format( "dc %s ", SPICE_VALUE( m_genDc->GetValue() ).ToSpiceString() );
    }
    catch( ... )
    {
        DisplayError( NULL, wxT( "Invalid DC value" ) );
        return false;
    }

    try
    {
        if( !empty( m_genAcMag ) )
        {
            acdc += wxString::Format( "ac %s ", SPICE_VALUE( m_genAcMag->GetValue() ).ToSpiceString() );

            if( !empty( m_genAcPhase ) )
                acdc += wxString::Format( "%s ", SPICE_VALUE( m_genAcPhase->GetValue() ).ToSpiceString() );
        }
    }
    catch( ... )
    {
        DisplayError( NULL, wxT( "Invalid AC magnitude or phase" ) );
        return false;
    }

    /// Transient section
    if( page == m_pwrPulse )
    {
        if( !m_pwrPulse->Validate() )
            return false;

        genericProcessing = true;
        trans += "pulse";
        genericReqParamsCount = 2;
        genericControls = { m_pulseInit, m_pulseNominal, m_pulseDelay,
            m_pulseRise, m_pulseFall, m_pulseWidth, m_pulsePeriod };
    }


    else if( page == m_pwrSin )
    {
        if( !m_pwrSin->Validate() )
            return false;

        genericProcessing = true;
        trans += "sin";
        genericReqParamsCount = 2;
        genericControls = { m_sinOffset, m_sinAmplitude, m_sinFreq, m_sinDelay, m_sinDampFactor };
    }


    else if( page == m_pwrExp )
    {
        if( !m_pwrExp->Validate() )
            return false;

        genericProcessing = true;
        trans += "exp";
        genericReqParamsCount = 2;
        genericControls = { m_expInit, m_expPulsed,
            m_expRiseDelay, m_expRiseConst, m_expFallDelay, m_expFallConst };
    }


    else if( page == m_pwrPwl )
    {
        if( m_pwlValList->GetItemCount() > 0 )
        {
            trans += "pwl(";

            for( int i = 0; i < m_pwlValList->GetItemCount(); ++i )
            {
                trans += wxString::Format( "%s %s ", m_pwlValList->GetItemText( i, m_pwlTimeCol ),
                                                     m_pwlValList->GetItemText( i, m_pwlValueCol ) );
            }

            trans.Trim();
            trans += ")";
        }
    }

    if( genericProcessing )
    {
        trans += "(";

        auto first_empty = std::find_if( genericControls.begin(), genericControls.end(), empty );
        auto first_not_empty = std::find_if( genericControls.begin(), genericControls.end(),
                []( const wxTextCtrl* c ){ return !empty( c ); } );

        if( std::distance( first_not_empty, genericControls.end() ) == 0 )
        {
            // all empty
            useTrans = false;
        }
        else if( std::distance( genericControls.begin(), first_empty ) < (int)genericReqParamsCount )
        {
            DisplayError( nullptr,
                    wxString::Format( wxT( "You need to specify at least the "
                                           "first %d parameters for the transient source" ),
                            genericReqParamsCount ) );

            return false;
        }
        else if( std::find_if_not( first_empty, genericControls.end(),
                         empty ) != genericControls.end() )
        {
            DisplayError( nullptr, wxT( "You cannot leave interleaved empty fields "
                                        "when defining a transient source" ) );
            return false;
        }
        else
        {
            std::for_each( genericControls.begin(), first_empty,
                    [&trans] ( wxTextCtrl* ctrl ) {
                trans += wxString::Format( "%s ", ctrl->GetValue() );
            } );
        }

        trans.Trim();
        trans += ")";
    }

    aTarget = acdc;

    if( useTrans )
        aTarget += trans;

    // Remove whitespaces from left and right side
    aTarget.Trim( false );
    aTarget.Trim( true );

    return true;
}


void DIALOG_SPICE_MODEL::loadLibrary( const wxString& aFilePath )
{
    wxString curModel = m_modelName->GetValue();
    m_models.clear();
    wxFileName filePath( aFilePath );
    bool in_subckt = false;        // flag indicating that the parser is inside a .subckt section

    // Look for the file in the project path
    if( !filePath.Exists() )
    {
        filePath.SetPath( Prj().GetProjectPath() + filePath.GetPath() );

        if( !filePath.Exists() )
            return;
    }

    // Display the library contents
    wxWindowUpdateLocker updateLock( this );
    m_libraryContents->Clear();
    wxTextFile file;
    file.Open( filePath.GetFullPath() );
    int line_nr = 0;

    // Stores the libray content. It will be displayed after reading the full library
    wxString fullText;

    // Process the file, looking for components
    while( !file.Eof() )
    {
        const wxString& line = line_nr == 0 ? file.GetFirstLine() : file.GetNextLine();
        fullText << line << '\n';

        wxStringTokenizer tokenizer( line );

        while( tokenizer.HasMoreTokens() )
        {
            wxString token = tokenizer.GetNextToken().Lower();

            // some subckts contain .model clauses inside,
            // skip them as they are a part of the subckt, not another model
            if( token == ".model" && !in_subckt )
            {
                wxString name = tokenizer.GetNextToken();

                if( name.IsEmpty() )
                    break;

                token = tokenizer.GetNextToken();
                SPICE_PRIMITIVE type = MODEL::parseModelType( token );

                if( type != SP_UNKNOWN )
                    m_models.emplace( name, MODEL( line_nr, type ) );
            }

            else if( token == ".subckt" )
            {
                wxASSERT( !in_subckt );
                in_subckt = true;

                wxString name = tokenizer.GetNextToken();

                if( name.IsEmpty() )
                    break;

                m_models.emplace( name, MODEL( line_nr, SP_SUBCKT ) );
            }

            else if( token == ".ends" )
            {
                wxASSERT( in_subckt );
                in_subckt = false;
            }
        }

        ++line_nr;
    }

    // display the full library content:
    m_libraryContents->AppendText( fullText );

    wxArrayString modelsList;

    // Refresh the model name combobox values
    m_modelName->Clear();

    for( const auto& model : m_models )
    {
        m_modelName->Append( model.first );
        modelsList.Add( model.first );
    }

    m_modelName->AutoComplete( modelsList );

    // Restore the previous value or if there is none - pick the first one from the loaded library
    if( !curModel.IsEmpty() )
        m_modelName->SetValue( curModel );
    else if( m_modelName->GetCount() > 0 )
        m_modelName->SetSelection( 0 );
}


SCH_FIELD& DIALOG_SPICE_MODEL::getSchField( int aFieldType )
{
    const wxString& spiceField = NETLIST_EXPORTER_PSPICE::GetSpiceFieldName( (SPICE_FIELD) aFieldType );

    auto fieldIt = std::find_if( m_schfields->begin(), m_schfields->end(), [&]( const SCH_FIELD& f ) {
        return f.GetName() == spiceField;
    } );

    // Found one, so return it
    if( fieldIt != m_schfields->end() )
        return *fieldIt;

    // Create a new field with requested name
    m_schfields->emplace_back( wxPoint(), m_schfields->size(), &m_component, spiceField );
    return m_schfields->back();
}


LIB_FIELD& DIALOG_SPICE_MODEL::getLibField( int aFieldType )
{
    const wxString& spiceField = NETLIST_EXPORTER_PSPICE::GetSpiceFieldName( (SPICE_FIELD) aFieldType );

    auto fieldIt = std::find_if( m_libfields->begin(), m_libfields->end(), [&]( const LIB_FIELD& f ) {
        return f.GetName() == spiceField;
    } );

    // Found one, so return it
    if( fieldIt != m_libfields->end() )
        return *fieldIt;

    // Create a new field with requested name
    LIB_FIELD new_field( m_libfields->size() );
    m_libfields->front().Copy( &new_field );
    new_field.SetName( spiceField );

    m_libfields->push_back( new_field );
    return m_libfields->back();
}


bool DIALOG_SPICE_MODEL::addPwlValue( const wxString& aTime, const wxString& aValue )
{
    // TODO execute validators
    if( aTime.IsEmpty() || aValue.IsEmpty() )
        return false;

    long idx = m_pwlValList->InsertItem( m_pwlTimeCol, aTime );
    m_pwlValList->SetItem( idx, m_pwlValueCol, aValue );

    // There is no wxString::ToFloat, but we need to guarantee it fits in 4 bytes
    double timeD;
    float timeF;
    m_pwlTime->GetValue().ToDouble( &timeD );
    timeF = timeD;
    long data;
    std::memcpy( &data, &timeF, sizeof( timeF ) );

    // Store the time value, so the entries can be sorted
    m_pwlValList->SetItemData( idx, data );

    // Sort items by timestamp
    m_pwlValList->SortItems( comparePwlValues, -1 );

    return true;
}


void DIALOG_SPICE_MODEL::onSelectLibrary( wxCommandEvent& event )
{
    wxString searchPath = wxFileName( m_modelLibrary->GetValue() ).GetPath();

    if( searchPath.IsEmpty() )
        searchPath = Prj().GetProjectPath();

    wxString     wildcards = SpiceLibraryFileWildcard() + "|" + AllFilesWildcard();
    wxFileDialog openDlg( this, _( "Select library" ), searchPath, "", wildcards,
            wxFD_OPEN | wxFD_FILE_MUST_EXIST );

    if( openDlg.ShowModal() == wxID_CANCEL )
        return;

    wxFileName libPath( openDlg.GetPath() );

    // Try to convert the path to relative to project
    if( libPath.MakeRelativeTo( Prj().GetProjectPath() ) && !libPath.GetFullPath().StartsWith( ".." ) )
        m_modelLibrary->SetValue( libPath.GetFullPath() );
    else
        m_modelLibrary->SetValue( openDlg.GetPath() );

    loadLibrary( openDlg.GetPath() );
    m_modelName->Popup();
}


void DIALOG_SPICE_MODEL::onModelSelected( wxCommandEvent& event )
{
    // autoselect the model type
    auto it = m_models.find( m_modelName->GetValue() );

    if( it != m_models.end() )
    {
        m_modelType->SetSelection( getModelTypeIdx( it->second.model ) );

        // scroll to the bottom, so the model definition is shown in the first line
        m_libraryContents->ShowPosition(
                m_libraryContents->XYToPosition( 0, m_libraryContents->GetNumberOfLines() ) );
        m_libraryContents->ShowPosition( m_libraryContents->XYToPosition( 0, it->second.line ) );
    }
    else
    {
        m_libraryContents->ShowPosition( 0 );
    }
}


void DIALOG_SPICE_MODEL::onPwlAdd( wxCommandEvent& event )
{
    addPwlValue( m_pwlTime->GetValue(), m_pwlValue->GetValue() );
}


void DIALOG_SPICE_MODEL::onPwlRemove( wxCommandEvent& event )
{
    long idx = m_pwlValList->GetNextItem( -1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED );
    m_pwlValList->DeleteItem( idx );
}


SPICE_PRIMITIVE DIALOG_SPICE_MODEL::MODEL::parseModelType( const wxString& aValue )
{
    wxCHECK( !aValue.IsEmpty(), SP_UNKNOWN );
    const wxString val( aValue.Lower() );

    for( const auto& model : modelTypes )
    {
        for( const auto& keyword : model.keywords )
        {
            if( val.StartsWith( keyword ) )
                return model.type;
        }
    }

    return SP_UNKNOWN;
}
