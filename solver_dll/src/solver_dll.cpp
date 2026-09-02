#include "vdp_ocp_nlp.hpp"
#include "IpIpoptApplication.hpp"

using namespace Ipopt;

extern "C" __declspec(dllexport) int solve() {
    const SmartPtr<VdpOcp_NLP> nlp = new VdpOcp_NLP(100, 1.0);
    const SmartPtr<IpoptApplication> app = IpoptApplicationFactory();

    app->Options()->SetIntegerValue("print_level", 0);
    app->Options()->SetIntegerValue("file_print_level", 5);
    app->Options()->SetStringValue("output_file", "vdp_ocp_nlp.log");
    app->Options()->SetIntegerValue("file_print_level", 5);
    // app->Options()->SetIntegerValue("mumps_print_level", 3);

    ApplicationReturnStatus status = app->Initialize();
    if (status != Solve_Succeeded) {
        return static_cast<int>(status);
    }

    status = app->OptimizeTNLP(nlp);

    return static_cast<int>(status);
}
