#include "floorplan.h"

/*This program will try to use tree to organize shapes in sush a way as the rectangles are organized in a bigger reactangle
This is very important to circuits designs and organizing things on silicon The trees will be sliced.*/

int num_modules;              // # of input modules.
module_t* modules;            // Array for modules.

// Procedure: floorplan
// The major procedure of the floorplan.
// - initialize the slicing tree.
// - print the information of the slicing tree.
// - perform the optimization process.
void floorplan(const char file[], const char outfile[]) {

  printf("\n********************************** MP11 **********************************\n");

  // Read the modules from the given input file.
  read_modules(file);

  // Initialize the slicing tree.
  node_t* root = init_slicing_tree(NULL, 0);
  int num_nodes = (num_modules << 1) - 1;
  printf("Initial slicing tree: Root=%p, num_nodes=%d, num_modules=%d\n", root, num_nodes, num_modules);

  // Obtain the expression of the initial slicing tree.
  expression_unit_t* expression = (expression_unit_t*)calloc(num_nodes, sizeof(expression_unit_t));
  get_expression(root, num_nodes, expression);
  printf("Initial expression: ");
  pnt_expression(expression, num_nodes);
  double area = packing(expression, num_nodes);
  printf("Initial area: %.5e\n", area);
  draw_modules("init.png");
  free(expression);

  // Perform the optimization process.
  printf("Perform optimization...\n");
  area = optimize(root, num_nodes);
  pnt_modules();
  printf("Packing area = %.5e (has overlapped? %d (1:yes, 0:no))\n", area, is_overlapped());

  // Output your floorplan.
  printf("Draw floorplan to %s\n", outfile);
  draw_modules(outfile);

  printf("********************************** END ***********************************\n");
}

/*
The following methods are used to optimize a floorplan given a binary tree.
It creates a tree with internal nodes that are either cut vertical or horizontal.
The Following methods are implemented to create the best way to stack circuits without overlap.
It keeps remaking the optimization until it gets the smallest area.
*/

// Return 1 if the given slicing tree node is a leaf node, and 0 otherwise.
int is_leaf_node(node_t* ptr) {
	//if nothing branches off the node, then it is a leaf
	if (ptr->left == NULL && ptr->right == NULL)
		return 1;
	 return 0;
}

// Return 1 if the given slicing tree node is an internal node, and 0 otherwise.
int is_internal_node(node_t* ptr) {
	//if it has a left or right, it is internal
	if (ptr->left != NULL || ptr->right != NULL)
		return 1;
	 return 0;
}

// Return 1 if the given subtree rooted at node 'b' resides in the subtree rooted at node 'a'.
int is_in_subtree(node_t* a, node_t* b) {
	// base cases
	if (b == NULL)
		return 0;
	if (a == NULL)
		return 0;
	// Check the tree with root as current node
	if (a == b)
		return 1;
	//check for all the nodes coming off the parent
	if(is_in_subtree(a->left, b) || is_in_subtree(a->right, b))
		return 1;
	return 0;
}

// Rotate a module from a given leaf node of the slicing tree by 90 degree.
// That is, the height and the width of the modules are swapped.
void rotate(node_t* ptr) {
	module_t * temp = (module_t*)malloc(sizeof(module_t));
	//change the width and height
	temp->w = ptr->module->w;
	ptr->module->w = ptr->module->h;
	ptr->module->h = temp->w;
	free(temp);
}

// Change the cutline of a module in a given internal node of the slicing tree.
// If the original cutline is a vertical cutline, the resulting cutline should be changed to
// horizontal and vice versa.
void recut(node_t* ptr) {
	if(!is_internal_node(ptr)) return;
	assert(ptr->module == NULL && ptr->cutline != UNDEFINED_CUTLINE);
	//change the cutline
	if ((ptr == NULL)) {
		return;
	}
	if (ptr->cutline == H) {
		ptr->cutline = V;
	}
	else if (ptr->cutline == V) {
		ptr->cutline = H;
	}

	recut(ptr->left);
	recut(ptr->right);

}

// Swap the two modules between two given leaf nodes in the slicing tree.
void swap_module(node_t* a, node_t* b) {
	//node_t * temp = (node_t*)malloc(sizeof(node_t));
	if(!is_leaf_node(a) || !is_leaf_node(b)) return;
	assert(a->module != NULL && a->cutline == UNDEFINED_CUTLINE);
	assert(b->module != NULL && b->cutline == UNDEFINED_CUTLINE);
	//switch the modules for the two given nodes using a temp
     module_t* temp = (module_t*)malloc(sizeof(module_t));
     temp = a->module;
     a->module = b->module;
     b->module = temp;
}

// Swap the topology of two subtrees rooted at two given nodes of the slicing tree.
// The procedure applies "is_in_subtree" first to tell if any of the subtree belongs
// to a part of the other.
void swap_topology(node_t* a, node_t* b) {
	node_t * temp = (node_t*)malloc(sizeof(node_t));
	if(a == NULL || b == NULL) return;
	if(a->parent == NULL || b->parent == NULL) return;
	if(is_in_subtree(a, b) || is_in_subtree(b, a)) return;
	assert(a->parent != NULL && b->parent != NULL);
	//if it is already in the subtree, dont need to swap
	if (is_in_subtree(a, b) == 1)
		return;
	//change the parents to point to the the other node
	if(a->parent->left == a)
   		a->parent->left = b;
    else
		a->parent->right = b;
    if(b->parent->left == b)
      	b->parent->left = a;
    else
     	b->parent->right = a;
	//switch the parents to point to the opposite
	temp->parent = a->parent;
    a->parent = b->parent;
    b->parent = temp->parent;
}

// Procedure: get_expression
// Perform the post-order traversal on the given slicing tree and stores the polish expression
// into the given expression array. You should assume the expression array is pre-allocated with
// size N.
void get_expression(node_t* root, int N, expression_unit_t* expression) {
  int i;
  // Clear the expression.
  for(i=0; i<N; ++i) {
    expression[i].module = NULL;
    expression[i].cutline = UNDEFINED_CUTLINE;
  }

  // Obtain the expression using the postfix traversal.
  int nth = 0;
  postfix_traversal(root, &nth, expression);
}

// Procedure: postfix_traversal
// Perform the postfix traversal on the slicing tree and store the corresponding polish expression
// to the given expression array. You should use the pointer "nth" to find out the index of the
// expression array and write the data accordingly.
void postfix_traversal(node_t* ptr, int* nth, expression_unit_t* expression) {
	int i;
  if(ptr == NULL) return;

  // first go through left subtree
  postfix_traversal(ptr->left, nth, expression);

  // then go through right subtree
  postfix_traversal(ptr->right, nth, expression);
  i = *nth;
  // now deal with the node
  if (ptr->cutline != UNDEFINED_CUTLINE) {
	  expression[i].module = NULL;
	  expression[i].cutline = ptr->cutline;
	  *nth = *nth + 1;
  }
  else if(ptr->module != NULL){
	  expression[i].module = ptr->module;
	  expression[i].cutline = UNDEFINED_CUTLINE;
	  *nth = *nth + 1;
  }
}


node_t* init_slicing_tree(node_t* par, int n) {
	node_t * i = (node_t*)malloc(sizeof(node_t));
	node_t * right = (node_t*)malloc(sizeof(node_t));
	assert(n >= 0 && n < num_modules);
	//once you get to the end, put the last node as the left instead of the right node
	if (n == num_modules-1) {
		node_t * left = (node_t*)malloc(sizeof(node_t));
		left->cutline = UNDEFINED_CUTLINE;
		left->left = NULL;
		left->right = NULL;
		left->parent = par;
		left->module = &modules[n];
		return left;
	}
	//for the root, dont need to assign a parent to the root
	if (par == NULL) {
		right->cutline = UNDEFINED_CUTLINE;
		right->left = NULL;
		right->right = NULL;
		right->parent = i;
		right->module = &modules[n];
		i->parent = NULL;
		i->cutline = V;
		i->right = right;
		i->module = NULL;
		i->left = init_slicing_tree(i, n + 1);
		return i;
	}
	//for the rest, assign a parent and the right value
	if (par != NULL) {
		right->cutline = UNDEFINED_CUTLINE;
		right->left = NULL;
		right->right = NULL;
		right->parent = i;
		right->module = &modules[n];
		i->parent = par;
		i->cutline = V;
		i->right = right;
		i->module = NULL;
		//left is equal to recursive call
		i->left = init_slicing_tree(i, n + 1);
		return i;
	}
}

// Return 1 if any overlaps among modules, and 0 otherwise.
int is_overlapped() {
  int i, j;
  int llxi, llyi, urxi, uryi;
  int llxj, llyj, urxj, uryj;
  int flag = 0;
  for(i=0; i<num_modules; ++i) {
    llxi = modules[i].llx;
    llyi = modules[i].lly;
    urxi = llxi + modules[i].w;
    uryi = llyi + modules[i].h;
    for(j=i+1; j<num_modules; ++j) {
     llxj = modules[j].llx;
     llyj = modules[j].lly;
     urxj = llxj + modules[j].w;
     uryj = llyj + modules[j].h;

     int rxu = MIN(urxi, urxj);
     int ryu = MIN(uryi, uryj);
     int rxl = MAX(llxi, llxj);
     int ryl = MAX(llyi, llyj);

     if(rxu>rxl && ryu>ryl) {
       flag = 1;
       printf("module %d and %d overlaps.\n", modules[i].idx, modules[j].idx);
     }
    }
  }
  return flag;
}

// Print the expression to the display.
void pnt_expression(expression_unit_t *expression, int N) {

  if(!is_valid_expression(expression, N)) {
    printf("Invalid expression. Can't print. Please check your get_expression procedure.\n");
    return;
  }

  int i;
  for(i=0; i<N; ++i) {
    if(expression[i].cutline == UNDEFINED_CUTLINE) {
      assert(expression[i].module != NULL);
      printf("%d", expression[i].module->idx);
    }
    else {
      assert(expression[i].module == NULL);
      printf("%c", expression[i].cutline == V ? 'V' : 'H');
    }
  }
  putchar('\n');
}

// Print the coordinates of each module.
void pnt_modules() {
  int i;
  for(i=0; i<num_modules; ++i) {
    printf("Module %d is placed at (%d, %d) with height=%d and width=%d\n",
            modules[i].idx,
            modules[i].llx,
            modules[i].lly,
            modules[i].h,
            modules[i].w);
  }
}

// Procedure: write_modules
// Write the coordinates of each module into a file.
void write_modules(const char file[]) {
  FILE *ofp = fopen(file, "w");
  int i;
  for(i=0; i<num_modules; ++i) {
    printf("%d %d %d %d %d\n",
            modules[i].idx,
            modules[i].llx,
            modules[i].lly,
            modules[i].h,
            modules[i].w);
  }
  fclose(ofp);
}

// Return the module pointer to the module with the given idx.
module_t* get_module(int idx) {
  int i;
  for(i=0; i<num_modules; ++i) {
    if(idx == modules[i].idx) return &modules[i];
  }
  return NULL;
}

// The main procedure of packing modules. The procedure takes the expression from the
// current slicing tree and then compute the coordinate of each module.
double packing(expression_unit_t* expression, int N) {

  if(!is_valid_expression(expression, N)) {
    //printf("Invalid expression. Can't pack. Please check your get_expression procedure.\n");
    return FLT_MAX;
  }

  // Initialize the stack by setting the variable stack_top as zero. The variable stack_top
  // is also denoting the size of the stack.
  int stack_top = 0;
  cluster_t* stack = (cluster_t*)malloc(N*sizeof(cluster_t));

  // Iterates through the expression
  int i, j;
  cluster_t cluster, cluster_l, cluster_r;
  for(i=0; i<N; ++i) {
    // Module
    if(expression[i].module != NULL) {
      assert(expression[i].cutline == UNDEFINED_CUTLINE);

      // Adjust the coordinate of the module.
      expression[i].module->llx = 0;
      expression[i].module->lly = 0;

      // Create a cluster.
      cluster.beg = i;
      cluster.end = i;
      cluster.w = expression[i].module->w;
      cluster.h = expression[i].module->h;
      stack[stack_top++] = cluster;
    }
    // Cutline
    else {
      assert(expression[i].cutline != UNDEFINED_CUTLINE);
      assert(stack_top >= 2);

      // Extract the top two clusters.
      cluster_r = stack[--stack_top];
      cluster_l = stack[--stack_top];

      // Create a new cluster.
      cluster.beg = cluster_l.beg;
      cluster.end = cluster_r.end;

      // Horizontal cutline.
      // - adjust the cluster's width and height.
      // - adjust the coordinates of modules from the right cluster.
      // - x coordinate doesn't change.
      if(expression[i].cutline == H) {
        for(j=cluster_r.beg; j<=cluster_r.end; ++j) {
          if(expression[j].module == NULL) continue;
          expression[j].module->lly += cluster_l.h;
        }
        cluster.w = cluster_l.w > cluster_r.w ? cluster_l.w : cluster_r.w;
        cluster.h = cluster_l.h + cluster_r.h;
      }
      // Vertical cutline.
      // - adjust the cluster's width and height.
      // - adjust the coordinates of modules from the right cluster.
      // - y coordinate doesn't change.
      else {
        for(j=cluster_r.beg; j<=cluster_r.end; ++j) {
          if(expression[j].module == NULL) continue;
          expression[j].module->llx += cluster_l.w;
        }
        cluster.w = cluster_l.w + cluster_r.w;
        cluster.h = cluster_l.h > cluster_r.h ? cluster_l.h : cluster_r.h;
      }

      // Insert the cluster into the stack.
      stack[stack_top++] = cluster;
    }
  }

  assert(stack_top == 1);

  double area = (double)stack[stack_top - 1].w * (double)stack[stack_top - 1].h;

  free(stack);

  return area;
}

// Function: is_valid_expression
int is_valid_expression(expression_unit_t* expression, int N) {

  // Initialize the stack by setting the variable stack_top as zero. The variable stack_top
  // is also denoting the size of the stack.
  int stack_top = 0;
  cluster_t* stack = (cluster_t*)malloc(N*sizeof(cluster_t));

  // Iterates through the expression
  int i;
  cluster_t cluster = {0, 0, 0, 0}, cluster_l, cluster_r;
  for(i=0; i<N; ++i) {
    // Module
    if(expression[i].module != NULL) {
      if(expression[i].cutline != UNDEFINED_CUTLINE) return 0;
      stack[stack_top++] = cluster;
    }
    // Cutline
    else {
      if(expression[i].cutline == UNDEFINED_CUTLINE) return 0;
      if(stack_top < 2) return 0;
      cluster_r = stack[--stack_top];
      cluster_l = stack[--stack_top];
      stack[stack_top++] = cluster;
    }
  }
  free(stack);
  return stack_top == 1;
}

// Read the modules from a given input file and initialize all required data structure.
void read_modules(const char file[]) {

  int i;
  FILE* ifp = fopen(file, "r");

  assert(ifp != NULL);

  // Read the number of modules.
  assert(fscanf(ifp, "%d", &num_modules) == 1);
  assert(num_modules >= 2);

  // Allocate the memory.
  modules = (module_t*)malloc(num_modules*sizeof(module_t));

  // Read the modules one by one.
  for(i=0; i<num_modules; ++i) {
    assert(fscanf(ifp, "%d %d %d", &modules[i].idx, &modules[i].w, &modules[i].h) == 3);
    modules[i].llx = 0;
    modules[i].lly = 0;
  }

  fclose(ifp);
}

// Procedure: copy_expression
void copy_expression(expression_unit_t* lhs, expression_unit_t* rhs, int N) {
  memcpy(lhs, rhs, N*sizeof(expression_unit_t));
}

// Procedure: copy_modules
void copy_modules(module_t* lhs, module_t* rhs, int N) {
  memcpy(lhs, rhs, N*sizeof(module_t));
}

// Accept the proposed solution.
int accept_proposal(double current, double proposal, double temperature) {
  if(proposal < current) return 1;
  if(temperature <= FROZEN) return 0;
  double prob = exp(-(double)(proposal - current) / temperature);
  return rand()/(double)RAND_MAX < prob;
}

// Function: get_rand_internal
node_t* get_rand_internal(node_t** internals, int num_internals) {
  return internals[rand()%num_internals];
}

// Function: get_rand_leaf
node_t* get_rand_leaf(node_t** leaves, int num_leaves) {
  return leaves[rand()%num_leaves];
}

// Function: get_random_node
node_t* get_rand_node(node_t** internals, int num_internals, node_t** leaves, int num_leaves) {
  if(rand()%2) {
    return get_rand_leaf(leaves, num_leaves);
  }
  return get_rand_internal(internals, num_internals);
}

// Optimize the area of the floorplanner.
double optimize(node_t *root, int num_nodes) {

  if(root == NULL) return FLT_MAX;

  // Storage for leaf and internal nodes.
  int head = 0;
  int tail = 0;
  int num_leaves = 0;
  int num_internals = 0;
  node_t** queue = (node_t**)malloc(num_nodes*sizeof(node_t*));
  node_t** leaves = (node_t**)malloc(num_modules*sizeof(node_t*));
  node_t** internals = (node_t**)malloc((num_modules-1)*sizeof(node_t*));
  node_t* u;
  queue[tail++] = root;

  while(tail - head) {
    u = queue[head++];

    if(u->module) {
      assert(u->cutline == UNDEFINED_CUTLINE);
      leaves[num_leaves++] = u;
    }
    else {
      assert(u->cutline != UNDEFINED_CUTLINE);
      internals[num_internals++] = u;
    }

    if(u->left) queue[tail++] = u->left;
    if(u->right) queue[tail++] = u->right;
  }

  assert(num_leaves == num_modules);
  assert(num_internals == num_modules - 1);

  // Simulated annealing.
  srand(time(0));
  expression_unit_t* best_expression = (expression_unit_t*)malloc(num_nodes*sizeof(expression_unit_t));
  expression_unit_t* curr_expression = (expression_unit_t*)malloc(num_nodes*sizeof(expression_unit_t));
  module_t* best_modules = (module_t*)malloc(num_modules*sizeof(module_t));

  int i, key;
  double best_area, curr_area;
  double temperature = 100.0;
  node_t* a;
  node_t* b;

  // Initialization.
  get_expression(root, num_nodes, best_expression);
  best_area = packing(best_expression, num_nodes);
  memcpy(best_modules, modules, num_modules*sizeof(module_t));

  while(temperature > FROZEN) {

    // Generate the neighboring solution.
    for(i=0; i<MAX_NUM_RAND_STEPS; ++i) {

      key = rand()%4;

      switch(key) {

        // Perform recut.
        case 0:
          recut(get_rand_internal(internals, num_internals));
        break;

        // Perform rotate.
        case 1:
          rotate(get_rand_leaf(leaves, num_leaves));
        break;

        // Perform swap_module.
        case 2:
          do {
            a = get_rand_leaf(leaves, num_leaves);
            b = get_rand_leaf(leaves, num_leaves);
          } while(a == b);
          swap_module(a, b);
        break;

        // Perform swap_topology.
        default:
          do {
            a = get_rand_node(internals, num_internals, leaves, num_leaves);
            b = get_rand_node(internals, num_internals, leaves, num_leaves);
          } while(is_in_subtree(a, b) || is_in_subtree(b, a));
          swap_topology(a, b);
        break;
      }

      // Evaluate the area.
      get_expression(root, num_nodes, curr_expression);
      curr_area = packing(curr_expression, num_nodes);
      if(curr_area < best_area) {
        best_area = curr_area;
        copy_expression(best_expression, curr_expression, num_nodes);
        memcpy(best_modules, modules, num_modules*sizeof(module_t));
      }
    }
    temperature *= TEMPERATURE_DECREASING_RATE;
  }

  memcpy(modules, best_modules, num_modules*sizeof(module_t));
  best_area = packing(best_expression, num_nodes);



  free(queue);
  free(leaves);
  free(internals);
  free(best_expression);
  free(curr_expression);
  free(best_modules);

  return best_area;
}

// Draw the modules using Cairo library.
void draw_modules(const char file[]) {

  int i;
  int max_x = 0, max_y = 0;
  for(i=0; i<num_modules; ++i) {
    if(modules[i].llx + modules[i].w > max_x) max_x = modules[i].llx + modules[i].w;
    if(modules[i].lly + modules[i].h > max_y) max_y = modules[i].lly + modules[i].h;
  }

  tLib::Graphics::Graphics2D design(max_x, max_y);

  for(i=0; i<num_modules; ++i) {
    design.set_painter_width(5);                          // set the line width.
    design.set_painter_color(0, 0, 1);                      // set the r, g, b color.
    design.paint_rectangle(modules[i].llx, modules[i].lly, modules[i].w, modules[i].h);
    //design.paint_text((double)(modules[i].llx) +( modules[i].w)/2.0,
    //                  (double)(modules[i].lly) + (modules[i].h)/2.0,
    //                   "A");
  }

  design.write_to_png(file);           // output the file to png image.
}
