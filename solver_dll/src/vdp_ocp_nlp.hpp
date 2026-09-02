#ifndef VDP_OCP_NLP_HPP
#define VDP_OCP_NLP_HPP

#include "IpTNLP.hpp"
#include <vector>

// Direct-transcription (trapezoidal collocation) NLP for the Van der Pol
// optimal control problem:
//
//   minimize   integral_0^T ( x1^2 + x2^2 + u^2 ) dt
//   subject to x1' = x2
//              x2' = (1 - x1^2) * x2 - x1 + u
//              x1(0) = x10, x2(0) = x20
//              -1 <= u(t) <= 1
//
// Discretized on N intervals (N+1 nodes) via trapezoidal collocation:
//
//   x1_{k+1} - x1_k - dt/2 * ( x2_k + x2_{k+1} )                     = 0
//   x2_{k+1} - x2_k - dt/2 * ( f2_k + f2_{k+1} )                     = 0
//
// where f2_j = (1 - x1_j^2) * x2_j - x1_j + u_j.
//
// This is a classic optimal-control benchmark (part of the COPS test set)
// and, unlike a replicated/block problem, gives a genuinely banded
// (block-tridiagonal) KKT structure that scales the way real large-scale
// IPOPT problems do.
//
// Variable layout: for node k = 0..N, x[3k+0]=x1_k, x[3k+1]=x2_k, x[3k+2]=u_k
// so n = 3*(N+1).
// Constraint layout: for interval k = 0..N-1, row 2k = defect1_k,
// row 2k+1 = defect2_k, so m = 2*N.
class VdpOcp_NLP final : public Ipopt::TNLP {
public:
    VdpOcp_NLP(Ipopt::Index N, Ipopt::Number T,
               Ipopt::Number x10 = 0.0, Ipopt::Number x20 = 1.0);
    ~VdpOcp_NLP() override = default;

    bool get_nlp_info(Ipopt::Index& n, Ipopt::Index& m, Ipopt::Index& nnz_jac_g,
                       Ipopt::Index& nnz_h_lag,
                       Ipopt::TNLP::IndexStyleEnum& index_style) override;

    bool get_bounds_info(Ipopt::Index n, Ipopt::Number* x_l, Ipopt::Number* x_u,
                          Ipopt::Index m, Ipopt::Number* g_l, Ipopt::Number* g_u) override;

    bool get_starting_point(Ipopt::Index n, bool init_x, Ipopt::Number* x,
                             bool init_z, Ipopt::Number* z_L, Ipopt::Number* z_U,
                             Ipopt::Index m, bool init_lambda, Ipopt::Number* lambda) override;

    bool eval_f(Ipopt::Index n, const Ipopt::Number* x, bool new_x,
                Ipopt::Number& obj_value) override;

    bool eval_grad_f(Ipopt::Index n, const Ipopt::Number* x, bool new_x,
                      Ipopt::Number* grad_f) override;

    bool eval_g(Ipopt::Index n, const Ipopt::Number* x, bool new_x,
                Ipopt::Index m, Ipopt::Number* g) override;

    bool eval_jac_g(Ipopt::Index n, const Ipopt::Number* x, bool new_x,
                     Ipopt::Index m, Ipopt::Index nele_jac,
                     Ipopt::Index* iRow, Ipopt::Index* jCol, Ipopt::Number* values) override;

    bool eval_h(Ipopt::Index n, const Ipopt::Number* x, bool new_x,
                Ipopt::Number obj_factor, Ipopt::Index m, const Ipopt::Number* lambda,
                bool new_lambda, Ipopt::Index nele_hess,
                Ipopt::Index* iRow, Ipopt::Index* jCol, Ipopt::Number* values) override;

    void finalize_solution(Ipopt::SolverReturn status, Ipopt::Index n, const Ipopt::Number* x,
                            const Ipopt::Number* z_L, const Ipopt::Number* z_U,
                            Ipopt::Index m, const Ipopt::Number* g, const Ipopt::Number* lambda,
                            Ipopt::Number obj_value, const Ipopt::IpoptData* ip_data,
                            Ipopt::IpoptCalculatedQuantities* ip_cq) override;

    bool success = false;
    Ipopt::Number solution_obj = 0.0;
    std::vector<Ipopt::Number> solution_x; // length 3*(N+1), same layout as above

private:
    Ipopt::Index N_;      // number of intervals (N+1 nodes)
    Ipopt::Number T_;     // horizon length
    Ipopt::Number dt_;    // T / N
    Ipopt::Number x10_, x20_;

    static Ipopt::Index idx(Ipopt::Index k, Ipopt::Index comp) { return 3 * k + comp; }
    Ipopt::Number quad_weight(Ipopt::Index k) const
    {
        return (k == 0 || k == N_) ? 0.5 * dt_ : dt_;
    }
};

#endif // VDP_OCP_NLP_HPP
