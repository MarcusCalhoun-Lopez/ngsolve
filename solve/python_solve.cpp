#ifdef NGS_PYTHON
#include "../ngstd/python_ngstd.hpp"
#include <solve.hpp>
using namespace ngsolve;



extern void ExportBVP();
extern void ExportDrawFlux();

void NGS_DLL_HEADER ExportNgsolve() {
    std::string nested_name = "solve";
    if( bp::scope() )
      nested_name = bp::extract<std::string>(bp::scope().attr("__name__") + ".solve");
    
    bp::object module(bp::handle<>(bp::borrowed(PyImport_AddModule(nested_name.c_str()))));

    cout << "exporting solve as " << nested_name << endl;
    bp::object parent = bp::scope() ? bp::scope() : bp::import("__main__");
    parent.attr("solve") = module ;

    bp::scope local_scope(module);

    bp::def ("Tcl_Eval", &Ng_TclCmd);

    bp::def ("Redraw", 
             FunctionPointer([](bool blocking) {Ng_Redraw(blocking);}),
             (bp::arg("blocking")=false)
             );


    bp::def ("Draw", FunctionPointer([](shared_ptr<MeshAccess> mesh) 
                                     {
                                       mesh->SelectMesh();
                                       Ng_TclCmd ("set ::selectvisual mesh;\n");
                                     })
             );


    bp::def("SetVisualization", FunctionPointer
            ([](bp::object deformation)
             {
               bool need_redraw = false;
               if (bp::extract<bool>(deformation).check())
                 {
                   bool def = bp::extract<bool>(deformation)();
                   Ng_TclCmd ("set ::visoptions.deformation "+ToString(def)+";\n");
                   Ng_TclCmd ("Ng_Vis_Set parameters;\n");
                   need_redraw = true;
                 }
               if (need_redraw)
                 Ng_Redraw(true);
             }),
            (bp::arg("deformation")=bp::object())
            )
      ;
    
    bp::def ("Draw", FunctionPointer
             ([](shared_ptr<GridFunction> gf, int sd, bool autoscale, double min, double max) 
              {
                gf->GetMeshAccess()->SelectMesh();
                Visualize (gf, gf->GetName());
                if (gf->Dimension() == 1)
                  Ng_TclCmd (string("set ::visoptions.scalfunction ")+gf->GetName()+":1;\n");
                else
                  Ng_TclCmd (string("set ::visoptions.vecfunction ")+gf->GetName()+";\n");
                Ng_TclCmd (string("set ::visoptions.subdivisions ")+ToString(sd)+";\n");
		Ng_TclCmd ("set ::visoptions.autoscale "+ToString(autoscale)+";\n");
		if(!autoscale){
		  Ng_TclCmd ("set ::visoptions.mminval "+ToString(min)+";\n");
		  Ng_TclCmd ("set ::visoptions.mmaxval "+ToString(max)+";\n");
		}
		Ng_TclCmd ("Ng_Vis_Set parameters;\n");
                Ng_TclCmd ("set ::selectvisual solution;\n");
              }),
             (bp::arg("gf"),bp::arg("sd")=2,bp::arg("autoscale")=true,
	      bp::arg("min")=0.0,bp::arg("max")=1.0)
             );

    
    bp::def ("Draw", FunctionPointer
             ([](shared_ptr<CoefficientFunction> cf, shared_ptr<MeshAccess> ma, string name,
                 bool draw_vol, bool draw_surf) 
              {
                ma->SelectMesh();
                netgen::SolutionData * vis = new VisualizeCoefficientFunction (ma, cf);
                Ng_SolutionData soldata;
                Ng_InitSolutionData (&soldata);
  
                soldata.name = (char*)name.c_str();
                soldata.data = 0;
                soldata.components = cf -> Dimension();
                if (cf->IsComplex()) soldata.components *= 2;
                soldata.iscomplex = cf -> IsComplex();
                soldata.draw_surface = draw_surf;
                soldata.draw_volume  = draw_vol; 
                /* 
                if (flags.GetDefineFlag("volume"))
                  soldata.draw_surface = false;
                if (flags.GetDefineFlag("boundary"))
                  soldata.draw_volume = false;
                */
                soldata.dist = 1;
                soldata.soltype = NG_SOLUTION_VIRTUAL_FUNCTION;
                soldata.solclass = vis;
                Ng_SetSolutionData (&soldata);
                

                if (cf->Dimension() == 1)
                  Ng_TclCmd (string("set ::visoptions.scalfunction ")+name+":1;\n");
                else
                  if (cf->Dimension() == 3 || cf->Dimension() == ma->GetDimension())
                    Ng_TclCmd (string("set ::visoptions.vecfunction ")+name+";\n");
                Ng_TclCmd ("Ng_Vis_Set parameters;\n");
                Ng_TclCmd ("set ::selectvisual solution;\n");

              }),
             (bp::arg("cf"),bp::arg("mesh"),bp::arg("name"),bp::arg("draw_vol")=true,bp::arg("draw_surf")=true)
             );





    ExportBVP();
    ExportDrawFlux();
}



BOOST_PYTHON_MODULE(libsolve) {
  ExportNgsolve();
}


#endif