#include "linear_program_solver.h"
#include "../basic/logger.h"
#include "../basic/basic_types.h"
#include "../3rd_lpsolve/lp_lib.h"


bool LinearProgramSolver::_solve_LPSOLVE(const LinearProgram* program) {
	try {
		typedef Variable<double>			Variable;
		typedef LinearExpression<double>	Objective;
		typedef LinearConstraint<double>	Constraint;

		const std::vector<Variable>& variables = program->variables();
		if (variables.empty()) {
			std::cerr << "variable set is empty" << std::endl;
			return false;
		}

		/* Create a new LP model */
		lprec* lp = make_lp(0, variables.size());
		if (!lp) {
			Logger::err("-") << "error in creating a LP model" << std::endl;
			return false;
		}
		set_verbose(lp, SEVERE);

		// create variables
		for (std::size_t i = 0; i < variables.size(); ++i) {
			const Variable& var = variables[i];
			double lb = 0.0;
			double ub = 1.0;
			if (var.bound_type() == Variable::DOUBLE)
				var.get_bound(lb, ub);

			if (var.variable_type() == Variable::INTEGER)
				set_int(lp, i, TRUE);		/* sets variable i to binary */
			else if (var.variable_type() == Variable::BINARY)
				set_binary(lp, i, TRUE);	/* sets variable i to binary */
		}

		// set objective 

		// lp_solve uses 1-based arrays
		// The LP_SOLVE manual says the first element is ignored, so any value is OK.
		std::vector<double> row(variables.size() + 1, 0);
									
		const Objective& objective = program->objective();
		const std::unordered_map<std::size_t, double>& obj_coeffs = objective.coefficients();
		std::unordered_map<std::size_t, double>::const_iterator it = obj_coeffs.begin();
		for (; it != obj_coeffs.end(); ++it) {
			std::size_t var_idx = it->first;
			double coeff = it->second;	
			row[var_idx + 1] = coeff;	// lp_solve uses 1-based arrays
		}
		set_obj_fn(lp, row.data());

		// Add constraints

		const std::vector<Constraint>& constraints = program->constraints();

		/* num constraints will be added, so allocate memory for it in advance to make things faster */
		resize_lp(lp, constraints.size(), get_Ncolumns(lp));

		set_add_rowmode(lp, TRUE);

		for (std::size_t i = 0; i < constraints.size(); ++i) {
			std::vector<int> colno;
			std::vector<double> sparserow;

			const Constraint& cstr = constraints[i];
			const std::unordered_map<std::size_t, double>& cstr_coeffs = cstr.coefficients();
			std::unordered_map<std::size_t, double>::const_iterator cur = cstr_coeffs.begin();
			for (; cur != cstr_coeffs.end(); ++cur) {
				std::size_t var_idx = cur->first;
				double coeff = cur->second;
				colno.push_back(var_idx + 1);	 // lp_solve uses 1-based arrays
				sparserow.push_back(coeff);
			}

			switch (cstr.bound_type())
			{
			case Constraint::FIXED:
				add_constraintex(lp, static_cast<int>(colno.size()), sparserow.data(), colno.data(), EQ, cstr.get_bound());
				break;
			case Constraint::LOWER:
				add_constraintex(lp, static_cast<int>(colno.size()), sparserow.data(), colno.data(), GE, cstr.get_bound());
				break;
			case Constraint::UPPER:
				add_constraintex(lp, static_cast<int>(colno.size()), sparserow.data(), colno.data(), LE, cstr.get_bound());
				break;
			case Constraint::DOUBLE: {
				double lb, ub;
				cstr.get_bound(lb, ub);
				add_constraintex(lp, static_cast<int>(colno.size()), sparserow.data(), colno.data(), GE, lb);
				add_constraintex(lp, static_cast<int>(colno.size()), sparserow.data(), colno.data(), LE, ub);
				break;
				}
			default:
				break;
			}
		}

		set_add_rowmode(lp, FALSE);
		int ret = ::solve(lp);
		switch (ret) {
		case 0:
// 			double objval = get_objective(lp);
// 			Logger::out("-") << "done. objective: " << objval << std::endl;
			result_.resize(variables.size());
			get_variables(lp, result_.data());
			break;
		case -2:
			Logger::err("-") << "Out of memory" << std::endl;
			break;
		case 1:
			Logger::err("-") << "The model is sub-optimal. Only happens if there are integer variables and there is already an integer solution found. The solution is not guaranteed the most optimal one." << std::endl;
			break;
		case 2:
			Logger::err("-") << "The model is infeasible" << std::endl;
			break;
		case 3:
			Logger::err("-") << "The model is unbounded" << std::endl;
			break;
		case 4:
			Logger::err("-") << "The model is degenerative" << std::endl;
			break;
		case 5:
			Logger::err("-") << "Numerical failure encountered" << std::endl;
			break;
		case 6:
			Logger::err("-") << "The abort() routine was called" << std::endl;
			break;
		case 7:
			Logger::err("-") << "A timeout occurred" << std::endl;
			break;
		case 9:
			Logger::err("-") << "The model could be solved by presolve. This can only happen if presolve is active via set_presolve()" << std::endl;
			break;
		case 25:
			Logger::err("-") << "Accuracy error encountered" << std::endl;
			break;
		default:
			break;
		}

		delete_lp(lp);
	}
	catch (std::exception e) {
		Logger::err("-") << "Error code = " << e.what() << std::endl;
	}
	catch (...) {
		Logger::err("-") << "Exception during optimization" << std::endl;
	}

	return false;
}

