// alembicPlugin
// Initial code generated by Softimage SDK Wizard
// Executed Fri Aug 19 09:14:49 UTC+0200 2011 by helge
// 
// Tip: You need to compile the generated code before you can load the plug-in.
// After you compile the plug-in, you can load it by clicking Update All in the Plugin Manager.
#include "stdafx.h"
#include "arnoldHelpers.h" 

using namespace XSI; 
using namespace MATH; 

#include "AlembicLicensing.h"

#include "AlembicWriteJob.h"
#include "AlembicPoints.h"
#include "AlembicCurves.h"
#include "AlembicPolyMsh.h"
#include "CommonProfiler.h"
#include "CommonMeshUtilities.h"
#include "CommonUtilities.h"
#include "AlembicPropertyNodes.h"

SICALLBACK XSILoadPlugin_2( PluginRegistrar& in_reg ) ;

SICALLBACK XSILoadPlugin( PluginRegistrar& in_reg )
{
	in_reg.PutAuthor(L"Exocortex Technologies, Inc and Helge Mathee");
	in_reg.PutName(L"ExocortexAlembicSoftimage");
	in_reg.PutEmail(L"support@exocortex.com");
	in_reg.PutURL(L"http://www.exocortex.com/alembic");

	// sync the softimage plugin version with the Crate version ---------------
	//		soft_MAJOR <-- combination of crate_MAJOR and crate_MINOR
	//		soft_MINOR <-- crate_BUILD 
	//		(e.g. Crate version 1.1.134 resolves to Soft plugin version 11.134)
	long digits = 1;
	long pten = 10;
	while(pten <= crate_MINOR_VERSION)
	{
		pten *= 10;
		digits ++;
	}
	long soft_MAJOR = PLUGIN_MAJOR_VERSION * (long)(pow(10.0, digits) ) + PLUGIN_MINOR_VERSION;

	in_reg.PutVersion(soft_MAJOR, crate_BUILD_VERSION);


	// moved into Application/Plugins/ExocortexAlembicSoftimage_UI.py
	// --------------------------------------------------------------
	// in_reg.RegisterMenu(siMenuMainFileExportID,L"alembic_MenuExport",false,false);
	// in_reg.RegisterMenu(siMenuMainFileImportID,L"alembic_MenuImport",false,false);
	// in_reg.RegisterMenu(siMenuMainFileProjectID,L"alembic_MenuPathManager",false,false);
	// in_reg.RegisterMenu(siMenuMainFileProjectID,L"alembic_ProfileStats",false,false);
	// in_reg.RegisterMenu(siMenuTbGetPropertyID,L"alembic_MenuMetaData",false,false);


	//if( HasAlembicWriterLicense() ) {
		in_reg.RegisterCommand(L"alembic_export",L"alembic_export");
      in_reg.RegisterCommand(L"alembic_export_jobs",L"alembic_export_jobs");

		in_reg.RegisterProperty(L"alembic_export_settings");
	//}

	//if( HasAlembicReaderLicense() ) {
		in_reg.RegisterCommand(L"alembic_import",L"alembic_import");
      in_reg.RegisterCommand(L"alembic_import_jobs",L"alembic_import_jobs");
		in_reg.RegisterCommand(L"alembic_attach_metadata",L"alembic_attach_metadata");
		in_reg.RegisterCommand(L"alembic_create_item",L"alembic_create_item");
		in_reg.RegisterCommand(L"alembic_path_manager",L"alembic_path_manager");
		in_reg.RegisterCommand(L"alembic_profile_stats",L"alembic_profile_stats");
        in_reg.RegisterCommand(L"alembic_get_nodes",L"alembic_get_nodes");
        in_reg.RegisterCommand(L"alembic_get_paths",L"alembic_get_paths");
        in_reg.RegisterCommand(L"alembic_replace_path",L"alembic_replace_path");
        

		in_reg.RegisterOperator(L"alembic_xform");
		in_reg.RegisterOperator(L"alembic_camera");
		in_reg.RegisterOperator(L"alembic_polymesh");
		in_reg.RegisterOperator(L"alembic_polymesh_topo");
		in_reg.RegisterOperator(L"alembic_nurbs");
		in_reg.RegisterOperator(L"alembic_bbox");
		in_reg.RegisterOperator(L"alembic_normals");
		in_reg.RegisterOperator(L"alembic_uvs");
		in_reg.RegisterOperator(L"alembic_crvlist");
		in_reg.RegisterOperator(L"alembic_crvlist_topo");
		in_reg.RegisterOperator(L"alembic_visibility");
		in_reg.RegisterOperator(L"alembic_geomapprox");
		in_reg.RegisterOperator(L"alembic_standinop");


		in_reg.RegisterProperty(L"alembic_import_settings");
		in_reg.RegisterProperty(L"alembic_timecontrol");
		in_reg.RegisterProperty(L"alembic_metadata");

		// register ICE nodes
		Register_alembic_curves(in_reg);
		Register_alembic_points(in_reg);
		Register_alembic_polyMesh(in_reg);
        Register_alembic_string_array(in_reg);
        Register_alembic_float_array(in_reg);
        Register_alembic_vec2f_array(in_reg);
        Register_alembic_vec3f_array(in_reg);
        Register_alembic_vec4f_array(in_reg);
        Register_alembic_int_array(in_reg);

		//XSILoadPlugin_2( in_reg );

		// register events
		in_reg.RegisterEvent(L"alembic_OnCloseScene",siOnCloseScene);
	//}

   ESS_LOG_INFO("PLUGIN loaded");

 	return CStatus::OK;
}

SICALLBACK XSIUnloadPlugin( const PluginRegistrar& in_reg )
{
   deleteAllArchives();

	CString strPluginName;
	strPluginName = in_reg.GetName();
	Application().LogMessage(strPluginName + L" has been unloaded.",siVerboseMsg);
	return CStatus::OK;
}




ESS_CALLBACK_START(alembic_MenuExport_Init,CRef&)
	Context ctxt( in_ctxt );
	Menu oMenu;
	oMenu = ctxt.GetSource();
	MenuItem oNewItem;
	oMenu.AddCommandItem(L"Alembic 1.1",L"alembic_export_jobs",oNewItem);
	return CStatus::OK;
ESS_CALLBACK_END

ESS_CALLBACK_START(alembic_MenuImport_Init,CRef&)
	Context ctxt( in_ctxt );
	Menu oMenu;
	oMenu = ctxt.GetSource();
	MenuItem oNewItem;
	oMenu.AddCommandItem(L"Alembic 1.1",L"alembic_import_jobs",oNewItem);
	return CStatus::OK;
ESS_CALLBACK_END

ESS_CALLBACK_START(alembic_MenuPathManager_Init,CRef&)
	Context ctxt( in_ctxt );
	Menu oMenu;
	oMenu = ctxt.GetSource();
	MenuItem oNewItem;
	oMenu.AddCommandItem(L"Alembic Path Manager",L"alembic_path_manager",oNewItem);
	return CStatus::OK;
ESS_CALLBACK_END

ESS_CALLBACK_START(alembic_ProfileStats_Init,CRef&)
	Context ctxt( in_ctxt );
	Menu oMenu;
	oMenu = ctxt.GetSource();
	MenuItem oNewItem;
	oMenu.AddCommandItem(L"Alembic Profile Stats",L"alembic_profile_stats",oNewItem);
	return CStatus::OK;
ESS_CALLBACK_END

ESS_CALLBACK_START(alembic_profile_stats_Init,CRef&)
	Context ctxt( in_ctxt );
	Command oCmd;
	oCmd = ctxt.GetSource();
	oCmd.PutDescription(L"");
	oCmd.EnableReturnValue(true);

	return CStatus::OK;
ESS_CALLBACK_END

ESS_CALLBACK_START(alembic_profile_stats_Execute, CRef&)
    ESS_PROFILE_REPORT();
	return CStatus::OK;
ESS_CALLBACK_END

ESS_CALLBACK_START(alembic_OnCloseScene_OnEvent,CRef&)
   deleteAllArchives();
	return CStatus::OK;
ESS_CALLBACK_END
