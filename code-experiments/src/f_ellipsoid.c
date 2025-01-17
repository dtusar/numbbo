#include <stdio.h>
#include <math.h>
#include <assert.h>

#include "coco.h"
#include "coco_problem.c"
#include "transform_vars_oscillate.c"
#include "transform_vars_affine.c"
#include "transform_vars_shift.c"
#include "transform_obj_shift.c"
#include "transform_vars_permblockdiag.c"

static double f_ellipsoid_raw(const double *x, const size_t number_of_variables) {

  static const double condition = 1.0e6;
  size_t i = 0;
  double result;

  result = x[i] * x[i];
  for (i = 1; i < number_of_variables; ++i) {
    const double exponent = 1.0 * (double) (long) i / ((double) (long) number_of_variables - 1.0);
    result += pow(condition, exponent) * x[i] * x[i];
  }

  return result;
}

static void f_ellipsoid_evaluate(coco_problem_t *self, const double *x, double *y) {
  assert(self->number_of_objectives == 1);
  y[0] = f_ellipsoid_raw(x, self->number_of_variables);
}

static coco_problem_t *f_ellipsoid_allocate(const size_t number_of_variables) {

  coco_problem_t *problem = coco_problem_allocate_from_scalars("ellipsoid function",
      f_ellipsoid_evaluate, NULL, number_of_variables, -5.0, 5.0, 0.0);
  coco_problem_set_id(problem, "%s_d%02lu", "ellipsoid", number_of_variables);

  /* Compute best solution */
  f_ellipsoid_evaluate(problem, problem->best_parameter, problem->best_value);
  return problem;
}

static coco_problem_t *f_ellipsoid_bbob_problem_allocate(const size_t function,
                                                         const size_t dimension,
                                                         const size_t instance,
                                                         const long rseed,
                                                         const char *problem_id_template,
                                                         const char *problem_name_template) {
  double *xopt, fopt;
  coco_problem_t *problem = NULL;

  xopt = coco_allocate_vector(dimension);
  fopt = bbob2009_compute_fopt(function, instance);
  bbob2009_compute_xopt(xopt, rseed, dimension);

  problem = f_ellipsoid_allocate(dimension);
  problem = f_transform_vars_oscillate(problem);
  problem = f_transform_vars_shift(problem, xopt, 0);
  problem = f_transform_obj_shift(problem, fopt);

  coco_problem_set_id(problem, problem_id_template, function, instance, dimension);
  coco_problem_set_name(problem, problem_name_template, function, instance, dimension);
  coco_problem_set_type(problem, "1-separable");

  coco_free_memory(xopt);
  return problem;
}

static coco_problem_t *f_ellipsoid_rotated_bbob_problem_allocate(const size_t function,
                                                                 const size_t dimension,
                                                                 const size_t instance,
                                                                 const long rseed,
                                                                 const char *problem_id_template,
                                                                 const char *problem_name_template) {
  double *xopt, fopt;
  coco_problem_t *problem = NULL;

  double *M = coco_allocate_vector(dimension * dimension);
  double *b = coco_allocate_vector(dimension);
  double **rot1;

  xopt = coco_allocate_vector(dimension);
  bbob2009_compute_xopt(xopt, rseed, dimension);
  fopt = bbob2009_compute_fopt(function, instance);

  rot1 = bbob2009_allocate_matrix(dimension, dimension);
  bbob2009_compute_rotation(rot1, rseed + 1000000, dimension);
  bbob2009_copy_rotation_matrix(rot1, M, b, dimension);
  bbob2009_free_matrix(rot1, dimension);

  problem = f_ellipsoid_allocate(dimension);
  problem = f_transform_vars_oscillate(problem);
  problem = f_transform_vars_affine(problem, M, b, dimension);
  problem = f_transform_vars_shift(problem, xopt, 0);
  problem = f_transform_obj_shift(problem, fopt);

  coco_problem_set_id(problem, problem_id_template, function, instance, dimension);
  coco_problem_set_name(problem, problem_name_template, function, instance, dimension);
  coco_problem_set_type(problem, "3-ill-conditioned");

  coco_free_memory(M);
  coco_free_memory(b);
  coco_free_memory(xopt);
  return problem;
}

static coco_problem_t *f_ellipsoid_permblockdiag_bbob_problem_allocate(const size_t function,
                                                                 const size_t dimension,
                                                                 const size_t instance,
                                                                 const long rseed,
                                                                 const char *problem_id_template,
                                                                 const char *problem_name_template) {
  double *xopt, fopt;
  coco_problem_t *problem = NULL;
  double **B;
  const double *const *B_copy;
  size_t *P1 = (size_t *)coco_allocate_memory(dimension * sizeof(size_t));/*TODO: implement a allocate_size_t_vector*/
  size_t *P2 = (size_t *)coco_allocate_memory(dimension * sizeof(size_t));
  size_t *block_sizes;
  size_t nb_blocks;
  size_t swap_range;
  size_t nb_swaps;
  
  block_sizes = ls_get_block_sizes(&nb_blocks, dimension);
  swap_range = ls_get_swap_range(dimension);
  nb_swaps = ls_get_nb_swaps(dimension);

  /*printf("f:%zu  n:%zu  i:%zu  bs:[%zu,...,%zu,%zu]  sR:%zu\n", function, dimension, instance, block_sizes[0], block_sizes[0],block_sizes[nb_blocks-1], swap_range);*/
  
  xopt = coco_allocate_vector(dimension);
  bbob2009_compute_xopt(xopt, rseed, dimension);
  fopt = bbob2009_compute_fopt(function, instance);
  
  B = ls_allocate_blockmatrix(dimension, block_sizes, nb_blocks);
  B_copy = (const double *const *)B;/*TODO: silences the warning, not sure if it prenvents the modification of B at all levels*/

  ls_compute_blockrotation(B, rseed + 1000000, dimension, block_sizes, nb_blocks);
  ls_compute_truncated_uniform_swap_permutation(P1, rseed + 2000000, dimension, nb_swaps, swap_range);
  ls_compute_truncated_uniform_swap_permutation(P2, rseed + 3000000, dimension, nb_swaps, swap_range);

  
  problem = f_ellipsoid_allocate(dimension);
  problem = f_transform_vars_oscillate(problem);
  problem = f_ls_transform_vars_permblockdiag(problem, B_copy, P1, P2, dimension, block_sizes, nb_blocks);
  problem = f_transform_vars_shift(problem, xopt, 0);
  problem = f_transform_obj_shift(problem, fopt);

  coco_problem_set_id(problem, problem_id_template, function, instance, dimension);
  coco_problem_set_name(problem, problem_name_template, function, instance, dimension);
  coco_problem_set_type(problem, "large_scale_block_rotated");/*TODO: no large scale prefix*/

  ls_free_block_matrix(B, dimension);
  coco_free_memory(P1);
  coco_free_memory(P2);
  coco_free_memory(block_sizes);
  
  return problem;
}




