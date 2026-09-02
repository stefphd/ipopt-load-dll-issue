#include "vdp_ocp_nlp.hpp"

using namespace Ipopt;

VdpOcp_NLP::VdpOcp_NLP(Index N, Number T, Number x10, Number x20)
    : N_(N), T_(T), dt_(T / static_cast<Number>(N)), x10_(x10), x20_(x20)
{
    solution_x.resize(3 * (N_ + 1), 0.0);
}

bool VdpOcp_NLP::get_nlp_info(Index& n, Index& m, Index& nnz_jac_g,
                               Index& nnz_h_lag, IndexStyleEnum& index_style)
{
    n = 3 * (N_ + 1);
    m = 2 * N_;

    // defect1_k touches 4 vars (x1_k, x2_k, x1_{k+1}, x2_{k+1});
    // defect2_k touches 6 vars (adds u_k, u_{k+1}).
    nnz_jac_g = N_ * (4 + 6);

    // Objective: diagonal (3 entries per node). Constraints: defect1 is
    // linear (no Hessian contribution); defect2 contributes 2 entries at
    // each of its two nodes (diagonal x1 term + off-diagonal x1/x2 term).
    nnz_h_lag = 3 * (N_ + 1) + 4 * N_;

    index_style = TNLP::C_STYLE;
    return true;
}

bool VdpOcp_NLP::get_bounds_info(Index /*n*/, Number* x_l, Number* x_u,
                                  Index /*m*/, Number* g_l, Number* g_u)
{
    for (Index k = 0; k <= N_; ++k) {
        x_l[idx(k, 0)] = -1e19; x_u[idx(k, 0)] = 1e19; // x1: free
        x_l[idx(k, 1)] = -1e19; x_u[idx(k, 1)] = 1e19; // x2: free
        x_l[idx(k, 2)] = -1.0;  x_u[idx(k, 2)] = 1.0;  // u: bounded control
    }

    // Fix the initial state.
    x_l[idx(0, 0)] = x_u[idx(0, 0)] = x10_;
    x_l[idx(0, 1)] = x_u[idx(0, 1)] = x20_;

    // All defect constraints are equalities: g = 0.
    for (Index k = 0; k < N_; ++k) {
        g_l[2 * k]     = 0.0; g_u[2 * k]     = 0.0;
        g_l[2 * k + 1] = 0.0; g_u[2 * k + 1] = 0.0;
    }
    return true;
}

bool VdpOcp_NLP::get_starting_point(Index /*n*/, bool init_x, Number* x,
                                     bool init_z, Number* /*z_L*/, Number* /*z_U*/,
                                     Index /*m*/, bool init_lambda, Number* /*lambda*/)
{
    if (init_x) {
        for (Index k = 0; k <= N_; ++k) {
            x[idx(k, 0)] = x10_;
            x[idx(k, 1)] = x20_;
            x[idx(k, 2)] = 0.0;
        }
    }
    if (init_z) return false;
    if (init_lambda) return false;
    return true;
}

bool VdpOcp_NLP::eval_f(Index /*n*/, const Number* x, bool /*new_x*/, Number& obj_value)
{
    obj_value = 0.0;
    for (Index k = 0; k <= N_; ++k) {
        const Number x1 = x[idx(k, 0)], x2 = x[idx(k, 1)], u = x[idx(k, 2)];
        obj_value += quad_weight(k) * (x1 * x1 + x2 * x2 + u * u);
    }
    return true;
}

bool VdpOcp_NLP::eval_grad_f(Index /*n*/, const Number* x, bool /*new_x*/, Number* grad_f)
{
    for (Index k = 0; k <= N_; ++k) {
        const Number w = quad_weight(k);
        grad_f[idx(k, 0)] = 2.0 * w * x[idx(k, 0)];
        grad_f[idx(k, 1)] = 2.0 * w * x[idx(k, 1)];
        grad_f[idx(k, 2)] = 2.0 * w * x[idx(k, 2)];
    }
    return true;
}

bool VdpOcp_NLP::eval_g(Index /*n*/, const Number* x, bool /*new_x*/, Index /*m*/, Number* g)
{
    for (Index k = 0; k < N_; ++k) {
        const Number x1k = x[idx(k, 0)],     x2k = x[idx(k, 1)],     uk = x[idx(k, 2)];
        const Number x1n = x[idx(k + 1, 0)], x2n = x[idx(k + 1, 1)], un = x[idx(k + 1, 2)];

        const Number f2k = (1.0 - x1k * x1k) * x2k - x1k + uk;
        const Number f2n = (1.0 - x1n * x1n) * x2n - x1n + un;

        g[2 * k]     = x1n - x1k - 0.5 * dt_ * (x2k + x2n);
        g[2 * k + 1] = x2n - x2k - 0.5 * dt_ * (f2k + f2n);
    }
    return true;
}

bool VdpOcp_NLP::eval_jac_g(Index /*n*/, const Number* x, bool /*new_x*/,
                             Index /*m*/, Index /*nele_jac*/,
                             Index* iRow, Index* jCol, Number* values)
{
    if (values == nullptr) {
        Index e = 0;
        for (Index k = 0; k < N_; ++k) {
            const Index r1 = 2 * k, r2 = 2 * k + 1;
            // defect1_k: x1_k, x2_k, x1_{k+1}, x2_{k+1}
            iRow[e] = r1; jCol[e] = idx(k, 0);     ++e;
            iRow[e] = r1; jCol[e] = idx(k, 1);     ++e;
            iRow[e] = r1; jCol[e] = idx(k + 1, 0); ++e;
            iRow[e] = r1; jCol[e] = idx(k + 1, 1); ++e;
            // defect2_k: x1_k, x2_k, u_k, x1_{k+1}, x2_{k+1}, u_{k+1}
            iRow[e] = r2; jCol[e] = idx(k, 0);     ++e;
            iRow[e] = r2; jCol[e] = idx(k, 1);     ++e;
            iRow[e] = r2; jCol[e] = idx(k, 2);     ++e;
            iRow[e] = r2; jCol[e] = idx(k + 1, 0); ++e;
            iRow[e] = r2; jCol[e] = idx(k + 1, 1); ++e;
            iRow[e] = r2; jCol[e] = idx(k + 1, 2); ++e;
        }
    } else {
        Index e = 0;
        for (Index k = 0; k < N_; ++k) {
            const Number x1k = x[idx(k, 0)],     x2k = x[idx(k, 1)];
            const Number x1n = x[idx(k + 1, 0)], x2n = x[idx(k + 1, 1)];

            // d(defect1_k)/d(...)
            values[e++] = -1.0;                 // x1_k
            values[e++] = -0.5 * dt_;            // x2_k
            values[e++] =  1.0;                 // x1_{k+1}
            values[e++] = -0.5 * dt_;            // x2_{k+1}

            // d(defect2_k)/d(...)   [f2_j = (1-x1_j^2)*x2_j - x1_j + u_j]
            values[e++] =  0.5 * dt_ * (2.0 * x1k * x2k + 1.0); // x1_k
            values[e++] = -1.0 - 0.5 * dt_ * (1.0 - x1k * x1k); // x2_k
            values[e++] = -0.5 * dt_;                            // u_k
            values[e++] =  0.5 * dt_ * (2.0 * x1n * x2n + 1.0); // x1_{k+1}
            values[e++] =  1.0 - 0.5 * dt_ * (1.0 - x1n * x1n); // x2_{k+1}
            values[e++] = -0.5 * dt_;                            // u_{k+1}
        }
    }
    return true;
}

bool VdpOcp_NLP::eval_h(Index /*n*/, const Number* x, bool /*new_x*/,
                         Number obj_factor, Index /*m*/, const Number* lambda,
                         bool /*new_lambda*/, Index /*nele_hess*/,
                         Index* iRow, Index* jCol, Number* values)
{
    // NOTE: IPOPT sums contributions from entries that share the same
    // (row, col) pair, so we don't need to hand-merge the objective's
    // diagonal with the constraints' contributions at shared nodes -- we
    // just emit each contribution as its own triplet.
    if (values == nullptr) {
        Index e = 0;
        // Objective: diagonal over (x1_k, x2_k, u_k) for every node.
        for (Index k = 0; k <= N_; ++k) {
            iRow[e] = idx(k, 0); jCol[e] = idx(k, 0); ++e;
            iRow[e] = idx(k, 1); jCol[e] = idx(k, 1); ++e;
            iRow[e] = idx(k, 2); jCol[e] = idx(k, 2); ++e;
        }
        // defect2_k nonlinear term contributes at node k and node k+1:
        // (x1,x1) diagonal and (x2,x1) off-diagonal (lower triangle).
        for (Index k = 0; k < N_; ++k) {
            iRow[e] = idx(k, 0);     jCol[e] = idx(k, 0);     ++e; // (x1_k,x1_k)
            iRow[e] = idx(k, 1);     jCol[e] = idx(k, 0);     ++e; // (x2_k,x1_k)
            iRow[e] = idx(k + 1, 0); jCol[e] = idx(k + 1, 0); ++e; // (x1_{k+1},x1_{k+1})
            iRow[e] = idx(k + 1, 1); jCol[e] = idx(k + 1, 0); ++e; // (x2_{k+1},x1_{k+1})
        }
    } else {
        Index e = 0;
        for (Index k = 0; k <= N_; ++k) {
            const Number w = quad_weight(k);
            values[e++] = obj_factor * 2.0 * w; // d2f/dx1_k^2
            values[e++] = obj_factor * 2.0 * w; // d2f/dx2_k^2
            values[e++] = obj_factor * 2.0 * w; // d2f/du_k^2
        }
        for (Index k = 0; k < N_; ++k) {
            const Number lam = lambda[2 * k + 1]; // multiplier of defect2_k
            const Number x1k = x[idx(k, 0)],     x2k = x[idx(k, 1)];
            const Number x1n = x[idx(k + 1, 0)], x2n = x[idx(k + 1, 1)];

            values[e++] = lam * (dt_ * x2k); // (x1_k,x1_k)
            values[e++] = lam * (dt_ * x1k); // (x2_k,x1_k)
            values[e++] = lam * (dt_ * x2n); // (x1_{k+1},x1_{k+1})
            values[e++] = lam * (dt_ * x1n); // (x2_{k+1},x1_{k+1})
        }
    }
    return true;
}

void VdpOcp_NLP::finalize_solution(SolverReturn status, Index n, const Number* x,
                                    const Number* /*z_L*/, const Number* /*z_U*/,
                                    Index /*m*/, const Number* /*g*/, const Number* /*lambda*/,
                                    Number obj_value, const IpoptData* /*ip_data*/,
                                    IpoptCalculatedQuantities* /*ip_cq*/)
{
    solution_x.assign(x, x + n);
    solution_obj = obj_value;
    success = (status == SUCCESS);
}
