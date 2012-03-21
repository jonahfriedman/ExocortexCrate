#include "Utility.h"
#include <xsi_application.h>
#include <xsi_x3dobject.h>
#include <xsi_model.h>
#include <xsi_operator.h>
#include <xsi_primitive.h>
#include <xsi_kinematicstate.h>
#include <xsi_kinematics.h>
#include <xsi_comapihandler.h>
#include <xsi_inputport.h>
#include <xsi_outputport.h>
#include <xsi_context.h>
#include <xsi_operatorcontext.h>
#include <xsi_customoperator.h>
#include <xsi_factory.h>
#include <xsi_parameter.h>
#include <xsi_ppglayout.h>
#include <xsi_ppgitem.h>
#include <xsi_shapekey.h>
#include <xsi_plugin.h>
#include <xsi_utils.h>
#include "AlembicLicensing.h"

using namespace XSI;

SampleInfo getSampleInfo
(
   double iFrame,
   Alembic::AbcCoreAbstract::TimeSamplingPtr iTime,
   size_t numSamps
)
{
   SampleInfo result;
   if (numSamps == 0)
      numSamps = 1;

   std::pair<Alembic::AbcCoreAbstract::index_t, double> floorIndex =
   iTime->getFloorIndex(iFrame, numSamps);

   result.floorIndex = floorIndex.first;
   result.ceilIndex = result.floorIndex;

   // check if we have a full license
   if(!HasAlembicReaderLicense())
   {
      if(result.floorIndex > 75)
      {
         EC_LOG_WARNING("[ExocortexAlembic] Reader license not found: Cannot open sample indices higher than 75.");
         result.floorIndex = 75;
         result.ceilIndex = 75;
         result.alpha = 0.0;
         return result;
      }
   }

   if (fabs(iFrame - floorIndex.second) < 0.0001) {
      result.alpha = 0.0f;
      return result;
   }

   std::pair<Alembic::AbcCoreAbstract::index_t, double> ceilIndex =
   iTime->getCeilIndex(iFrame, numSamps);

   if (fabs(iFrame - ceilIndex.second) < 0.0001) {
      result.floorIndex = ceilIndex.first;
      result.ceilIndex = result.floorIndex;
      result.alpha = 0.0f;
      return result;
   }

   if (result.floorIndex == ceilIndex.first) {
      result.alpha = 0.0f;
      return result;
   }

   result.ceilIndex = ceilIndex.first;

   result.alpha = (iFrame - floorIndex.second) / (ceilIndex.second - floorIndex.second);

   return result;
}

std::string getIdentifierFromRef(CRef in_Ref, bool includeHierarchy)
{
   std::string result;
   CRef ref = in_Ref;
   bool has3DObj = false;
   while(ref.IsValid())
   {
      Model model(ref);
      X3DObject obj(ref);
      ProjectItem item(ref);
      if(model.IsValid())
      {
         if(model.GetFullName() == Application().GetActiveSceneRoot().GetFullName())
            break;
         result = std::string("/")+ std::string(model.GetName().GetAsciiString()) + std::string("Xfo") + result;
         ref = model.GetModel().GetRef();
      }
      else if(obj.IsValid())
      {
         if(!has3DObj)
         {
            result = std::string("/")+ std::string(obj.GetName().GetAsciiString()) + result;
            has3DObj = true;
         }
         result = std::string("/")+ std::string(obj.GetName().GetAsciiString()) + std::string("Xfo") + result;
         if(includeHierarchy)
            ref = obj.GetParent3DObject().GetRef();
         else
            ref = obj.GetModel().GetRef();
      }
      else if(item.IsValid())
      {
         ref = item.GetParent3DObject().GetRef();
      }
   }
   return result;
}

XSI::CString truncateName(const XSI::CString & in_Name)
{
   CString name = in_Name;
   if(name.GetSubString(name.Length()-3,3).IsEqualNoCase(L"xfo"))
      name = name.GetSubString(0,name.Length()-3);
   return name;
}

CString getFullNameFromIdentifier(std::string in_Identifier)
{
   if(in_Identifier.length() == 0)
      return CString();
   CString mapped = nameMapGet(in_Identifier.c_str());
   if(!mapped.IsEmpty())
      return mapped;
   CStringArray parts = CString(in_Identifier.c_str()).Split(L"/");
   CString modelName = L"Scene_Root";
   CString objName = truncateName(parts[parts.GetCount()-1]);
   if(!objName.IsEqualNoCase(parts[parts.GetCount()-1]))
      return objName;
   if(parts.GetCount() > 2)
      modelName = truncateName(parts[parts.GetCount()-3]);
   return modelName+L"."+objName;
}

CRef getRefFromIdentifier(std::string in_Identifier)
{
   CRef result;
   result.Set(getFullNameFromIdentifier(in_Identifier));
   return result;
}

CRefArray getOperators( CRef in_Ref)
{
   Primitive primitive(in_Ref);
   CComAPIHandler comPrimitive( primitive.GetRef() );
   CComAPIHandler constructionHistory = comPrimitive.GetProperty( L"ConstructionHistory" );
   CValue valOperatorCollection;
   CValueArray args(3);
   args[1] = CValue( L"Operators" );
   constructionHistory.Call( L"Filter", valOperatorCollection, args );
   CComAPIHandler comOpColl = valOperatorCollection;
   CValue cnt = comOpColl.GetProperty( L"Count" );
   CRefArray ops( (LONG)cnt );
   for ( LONG i=0; i<(LONG)cnt; i++ )
   {
      CValue outOp;
      CValueArray itemsArgs;
      itemsArgs.Add( i );
      comOpColl.Invoke(L"Item", CComAPIHandler::PropertyGet, outOp, itemsArgs);
      ops[i] = outOp;
   }
   return ops;
}

std::map<std::string,bool> gIsRefAnimatedMap;
bool isRefAnimated(const CRef & in_Ref, bool xformCache)
{
   // check the cache
   std::map<std::string,bool>::iterator it = gIsRefAnimatedMap.find(in_Ref.GetAsText().GetAsciiString());
   if(it!=gIsRefAnimatedMap.end())
      return it->second;

   // convert to all types
   X3DObject x3d(in_Ref);
   Property prop(in_Ref);
   Primitive prim(in_Ref);
   KinematicState kineState(in_Ref);
   Operator op(in_Ref);
   ShapeKey shapeKey(in_Ref);

   if(x3d.IsValid())
   {
      // exclude the scene root model
      if(x3d.GetFullName() == Application().GetActiveSceneRoot().GetFullName())
         return returnIsRefAnimated(in_Ref,false);
      // check both kinematic states
      if(isRefAnimated(x3d.GetKinematics().GetLocal().GetRef()))
         return returnIsRefAnimated(in_Ref,true);
      if(isRefAnimated(x3d.GetKinematics().GetGlobal().GetRef()))
         return returnIsRefAnimated(in_Ref,true);
      // check all constraints
      CRefArray constraints = x3d.GetKinematics().GetConstraints();
      for(LONG i=0;i<constraints.GetCount();i++)
      {
         if(isRefAnimated(constraints[i]))
            return returnIsRefAnimated(in_Ref,true);
      }
      // check the parent
      if(isRefAnimated(x3d.GetParent()) && !xformCache)
         return returnIsRefAnimated(in_Ref,true);
   }
   else if(shapeKey.IsValid())
      return returnIsRefAnimated(in_Ref,true);
   else if(prop.IsValid())
      return returnIsRefAnimated(in_Ref,prop.IsAnimated());
   else if(prim.IsValid())
   {
      // first check if there are any envelopes
      if(prim.GetParent3DObject().GetEnvelopes().GetCount() > 0)
         return returnIsRefAnimated(in_Ref,true);
      // check if we have ice trees
      if(prim.GetICETrees().GetCount() > 0)
         return returnIsRefAnimated(in_Ref,true);
      // loop all ops on the construction history
      CRefArray ops = getOperators(prim.GetRef());
      for(LONG i=0;i<ops.GetCount();i++)
      {
         if(isRefAnimated(ops[i]))
            return returnIsRefAnimated(in_Ref,true);
      }
      if(prim.IsAnimated())
         return returnIsRefAnimated(in_Ref,true);
   }
   else if(kineState.IsValid())
   {
      // myself is animated?
      if(kineState.IsAnimated())
         return returnIsRefAnimated(in_Ref,true);
      CRef retargetOpRef;
      retargetOpRef.Set(kineState.GetFullName()+L".RetargetGlobalOp");
      if(retargetOpRef.IsValid())
         return returnIsRefAnimated(in_Ref,true);
   }
   else if(op.IsValid())
   {
      // check myself
      if(op.IsAnimated())
         return returnIsRefAnimated(in_Ref,true);
      CRefArray outputPorts = op.GetOutputPorts();
      CRefArray inputPorts = op.GetInputPorts();
      for(LONG i=0;i<inputPorts.GetCount();i++)
      {
         InputPort port(inputPorts[i]);
         if(!port.IsConnected())
            continue;
         CRef target = port.GetTarget();
         bool isOutput = false;
         // ensure this target is not an output
         for(LONG j=0;j<outputPorts.GetCount();j++)
         {
            OutputPort output(outputPorts[j]);
            if(output.GetTarget().GetAsText().IsEqualNoCase(target.GetAsText()))
            {
               isOutput = true;
               break;
            }
         }
         if(!isOutput)
         {
            // first check for kinematics
            KinematicState opKineState(target);
            if(opKineState.IsValid())
            {
               if(isRefAnimated(opKineState.GetParent3DObject().GetRef()))
                  return returnIsRefAnimated(in_Ref,true);
            }
            else if(isRefAnimated(target))
               return returnIsRefAnimated(in_Ref,true);
         }
      }
   }
   return returnIsRefAnimated(in_Ref,false);
}

bool returnIsRefAnimated(const XSI::CRef & in_Ref, bool animated)
{
   gIsRefAnimatedMap.insert(
      std::pair<std::string,bool>(
         in_Ref.GetAsText().GetAsciiString(),
         animated
      )
   );
   return animated;
}

void clearIsRefAnimatedCache()
{
   gIsRefAnimatedMap.clear();
}

std::map<std::string,std::string> gNameMap;
void nameMapAdd(CString identifier, CString name)
{
   std::map<std::string,std::string>::iterator it = gNameMap.find(identifier.GetAsciiString());
   if(it == gNameMap.end())
   {
      std::pair<std::string,std::string> pair(identifier.GetAsciiString(),name.GetAsciiString());
      gNameMap.insert(pair);
   }
}

CString nameMapGet(CString identifier)
{
   std::map<std::string,std::string>::iterator it = gNameMap.find(identifier.GetAsciiString());
   if(it == gNameMap.end())
      return L"";
   return it->second.c_str();
}

void nameMapClear()
{
   gNameMap.clear();
}

Alembic::Abc::ICompoundProperty getCompoundFromObject(Alembic::Abc::IObject object)
{
   const Alembic::Abc::MetaData &md = object.getMetaData();
   if(Alembic::AbcGeom::IXform::matches(md)) {
      return Alembic::AbcGeom::IXform(object,Alembic::Abc::kWrapExisting).getSchema();
   } else if(Alembic::AbcGeom::IPolyMesh::matches(md)) {
      return Alembic::AbcGeom::IPolyMesh(object,Alembic::Abc::kWrapExisting).getSchema();
   } else if(Alembic::AbcGeom::ICurves::matches(md)) {
      return Alembic::AbcGeom::ICurves(object,Alembic::Abc::kWrapExisting).getSchema();
   } else if(Alembic::AbcGeom::INuPatch::matches(md)) {
      return Alembic::AbcGeom::INuPatch(object,Alembic::Abc::kWrapExisting).getSchema();
   } else if(Alembic::AbcGeom::IPoints::matches(md)) {
      return Alembic::AbcGeom::IPoints(object,Alembic::Abc::kWrapExisting).getSchema();
   } else if(Alembic::AbcGeom::ISubD::matches(md)) {
      return Alembic::AbcGeom::ISubD(object,Alembic::Abc::kWrapExisting).getSchema();
   } else if(Alembic::AbcGeom::ICamera::matches(md)) {
      return Alembic::AbcGeom::ICamera(object,Alembic::Abc::kWrapExisting).getSchema();
   }
   return Alembic::Abc::ICompoundProperty();
}

Alembic::Abc::TimeSamplingPtr getTimeSamplingFromObject(Alembic::Abc::IObject object)
{
   const Alembic::Abc::MetaData &md = object.getMetaData();
   if(Alembic::AbcGeom::IXform::matches(md)) {
      return Alembic::AbcGeom::IXform(object,Alembic::Abc::kWrapExisting).getSchema().getTimeSampling();
   } else if(Alembic::AbcGeom::IPolyMesh::matches(md)) {
      return Alembic::AbcGeom::IPolyMesh(object,Alembic::Abc::kWrapExisting).getSchema().getTimeSampling();
   } else if(Alembic::AbcGeom::ICurves::matches(md)) {
      return Alembic::AbcGeom::ICurves(object,Alembic::Abc::kWrapExisting).getSchema().getTimeSampling();
   } else if(Alembic::AbcGeom::INuPatch::matches(md)) {
      return Alembic::AbcGeom::INuPatch(object,Alembic::Abc::kWrapExisting).getSchema().getTimeSampling();
   } else if(Alembic::AbcGeom::IPoints::matches(md)) {
      return Alembic::AbcGeom::IPoints(object,Alembic::Abc::kWrapExisting).getSchema().getTimeSampling();
   } else if(Alembic::AbcGeom::ISubD::matches(md)) {
      return Alembic::AbcGeom::ISubD(object,Alembic::Abc::kWrapExisting).getSchema().getTimeSampling();
   } else if(Alembic::AbcGeom::ICamera::matches(md)) {
      return Alembic::AbcGeom::ICamera(object,Alembic::Abc::kWrapExisting).getSchema().getTimeSampling();
   }
   return Alembic::Abc::TimeSamplingPtr();
}

size_t getNumSamplesFromObject(Alembic::Abc::IObject object)
{
   const Alembic::Abc::MetaData &md = object.getMetaData();
   if(Alembic::AbcGeom::IXform::matches(md)) {
      return Alembic::AbcGeom::IXform(object,Alembic::Abc::kWrapExisting).getSchema().getNumSamples();
   } else if(Alembic::AbcGeom::IPolyMesh::matches(md)) {
      return Alembic::AbcGeom::IPolyMesh(object,Alembic::Abc::kWrapExisting).getSchema().getNumSamples();
   } else if(Alembic::AbcGeom::ICurves::matches(md)) {
      return Alembic::AbcGeom::ICurves(object,Alembic::Abc::kWrapExisting).getSchema().getNumSamples();
   } else if(Alembic::AbcGeom::INuPatch::matches(md)) {
      return Alembic::AbcGeom::INuPatch(object,Alembic::Abc::kWrapExisting).getSchema().getNumSamples();
   } else if(Alembic::AbcGeom::IPoints::matches(md)) {
      return Alembic::AbcGeom::IPoints(object,Alembic::Abc::kWrapExisting).getSchema().getNumSamples();
   } else if(Alembic::AbcGeom::ISubD::matches(md)) {
      return Alembic::AbcGeom::ISubD(object,Alembic::Abc::kWrapExisting).getSchema().getNumSamples();
   } else if(Alembic::AbcGeom::ICamera::matches(md)) {
      return Alembic::AbcGeom::ICamera(object,Alembic::Abc::kWrapExisting).getSchema().getNumSamples();
   }
   return 0;
}

CStatus alembicOp_Define( CRef& in_ctxt )
{
   Context ctxt( in_ctxt );
   CustomOperator oCustomOperator;

   Parameter oParam;
   CRef oPDef;

   Factory oFactory = Application().GetFactory();
   oCustomOperator = ctxt.GetSource();

   oPDef = oFactory.CreateParamDef(L"muted",CValue::siBool,siAnimatable | siPersistable,L"muted",L"muted",0,0,1,0,1);
   oCustomOperator.AddParameter(oPDef,oParam);
   oPDef = oFactory.CreateParamDef(L"time",CValue::siFloat,siAnimatable | siPersistable,L"time",L"time",1,-100000,100000,0,1);
   oCustomOperator.AddParameter(oPDef,oParam);
   oPDef = oFactory.CreateParamDef(L"path",CValue::siString,siReadOnly | siPersistable,L"path",L"path",L"",L"",L"",L"",L"");
   oCustomOperator.AddParameter(oPDef,oParam);
   oPDef = oFactory.CreateParamDef(L"identifier",CValue::siString,siReadOnly | siPersistable,L"identifier",L"identifier",L"",L"",L"",L"",L"");
   oCustomOperator.AddParameter(oPDef,oParam);
   oPDef = oFactory.CreateParamDef(L"renderpath",CValue::siString,siReadOnly | siPersistable,L"renderpath",L"renderpath",L"",L"",L"",L"",L"");
   oCustomOperator.AddParameter(oPDef,oParam);
   oPDef = oFactory.CreateParamDef(L"renderidentifier",CValue::siString,siReadOnly | siPersistable,L"renderidentifier",L"renderidentifier",L"",L"",L"",L"",L"");
   oCustomOperator.AddParameter(oPDef,oParam);

   oCustomOperator.PutAlwaysEvaluate(false);
   oCustomOperator.PutDebug(0);

   return CStatus::OK;
}

CStatus alembicOp_DefineLayout( CRef& in_ctxt )
{
   Context ctxt( in_ctxt );
   PPGLayout oLayout;
   PPGItem oItem;
   oLayout = ctxt.GetSource();
   oLayout.Clear();
   oLayout.AddItem(L"muted",L"Muted");
   oLayout.AddItem(L"time",L"Time");
   oLayout.AddGroup(L"Preview");
   oLayout.AddItem(L"path",L"FilePath");
   oLayout.AddItem(L"identifier",L"Identifier");
   oLayout.EndGroup();
   oLayout.AddGroup(L"Render");
   oLayout.AddItem(L"renderpath",L"FilePath");
   oLayout.AddItem(L"renderidentifier",L"Identifier");
   oLayout.EndGroup();
   return CStatus::OK;
}

CStatus alembicOp_Term( CRef& in_ctxt )
{
   Context ctxt( in_ctxt );
   CustomOperator op(ctxt.GetSource());
   delRefArchive(op.GetParameterValue(L"path").GetAsText());
   return CStatus::OK;
}

int gHasStandinSupport = -1;
bool hasStandinSupport()
{
   if(gHasStandinSupport < 0)
   {
      gHasStandinSupport = 0;
      CRefArray plugins = Application().GetPlugins();
      for(LONG i=0;i<plugins.GetCount();i++)
      {
         Plugin plugin(plugins[i]);
         if(plugin.GetName().GetSubString(0,6).IsEqualNoCase(L"arnold"))
         {
            gHasStandinSupport = 1;
            break;
         }
      }
   }
   return gHasStandinSupport > 0;
}

CString getDSOPath()
{
   // first check the environment variables
   if(getenv("ArnoldAlembicDSO") != NULL)
   {
      std::string env = getenv("ArnoldAlembicDSO");
      if(!env.empty())
         return env.c_str();
   }

   CRefArray plugins = Application().GetPlugins();
   for(LONG i=0;i<plugins.GetCount();i++)
   {
      Plugin plugin(plugins[i]);
      if(plugin.GetName().IsEqualNoCase(L"ExocortexAlembicSoftimage"))
      {
         CString path = plugin.GetFilename();
         path = path.GetSubString(0,path.ReverseFindString(CUtils::Slash()));
         path = path.GetSubString(0,path.ReverseFindString(CUtils::Slash()));
#ifdef _WIN32
         path = CUtils::BuildPath(path,L"DSO",L"ExocortexAlembicArnold.dll");
#else
         path = CUtils::BuildPath(path,L"DSO",L"libExocortexAlembicArnold.so");
#endif
         return path;
      }
   }
#ifdef _DEBUG
   return L"C:\\development\\alembic\\build\\vs2008_x64\\arnoldalembic\\Debug\\ExocortexAlembicArnold.dll";
#endif
   return CString();
}