//method according to smith94x
//so far only implemented for the ensemble at constant volume and temperature
//NOTE: a reaction here is one trial move to dissociate one acid molecule to its dissociated form 
//so if the reaction is accepted then there is one more dissociated ion pair H+ and A-
//NOTE: generic_oneway_reaction does not break bonds for simple reactions. as long as there are no reactions like 2A -->B where one of the reacting A particles occurs in the polymer (think of bond breakages if the monomer in the polymer gets deleted in the reaction). This constraint is not of fundamental reason, but there would be a need for a rule for such "collision" reactions (a reaction like the one above).
//NOTE: paricle types have to start at one and have to increase by one for every type otherwise the function hide_particle cannot work correctly. Through adding 100 in the function hide_particle we ensure that the function works correctly if the particle types are monotonically increasing and if the largest particle type is smaller than 100.

#include "reaction_ensemble.hpp"
#include "random.hpp" //for random numbers
#include "energy.hpp"	//for energies
#include "external_potential.hpp" //for energies
#include "global.hpp" //for access to global variables
#include "particle_data.hpp" //for particle creation, modification
#include "statistics.hpp" //for distto
#include "integrate.hpp" //for time_step
#include <stdlib.h>  // for qsort()
#include <stdio.h> //for getline()
#include "utils.hpp" // for PI

reaction_system current_reaction_system={.nr_single_reactions=0, .reactions=NULL , .type_index=NULL, .nr_different_types=0, .charges_of_types=NULL, .water_type=-100, .standard_pressure_in_simulation_units=-10, .given_length_in_SI_units=-10, .given_length_in_simulation_units=-10, .temperature_reaction_ensemble=-10.0, .exclusion_radius=0.0}; //initialize watertype to negative number, for checking wether it has been assigned, the standard_pressure_in_simulation_units is an input parameter for the reaction ensemble


//declaration of boring helper functions
float factorial_Ni0_divided_by_factorial_Ni0_plus_nu_i(int Ni0, int nu_i);
int calculate_nu_bar(int* educt_coefficients, int len_educt_types,  int* product_coefficients, int len_product_types); //should only be used at when defining a new reaction
bool all_educt_particles_exist(int reaction_id);
int generic_oneway_reaction(int reaction_id);
int find_index_of_type(int type);
int replace(int p_id, int desired_type);
double conversion_factor_molar_concentration_to_number_concentration_per_simulation_box_volume(double given_length_in_simulation_units, double given_length_in_SI_units);
int create_particle(int desired_type);
int vec_random(double* vecrandom, double desired_length);
int hide_particle(int p_id, int previous_type);
int delete_particle (int p_id);
int intcmp(const void *aa, const void *bb);
void free_wang_landau();
void remove_bins_that_have_not_been_sampled();


int create_current_reaction_system_struct(){
	single_reaction** reactions_list=(single_reaction**) malloc(sizeof(single_reaction*));
	current_reaction_system.reactions=reactions_list;
	return 0;
}

int do_reaction(){
	int reaction_id=i_random(current_reaction_system.nr_single_reactions);
	generic_oneway_reaction(reaction_id);
	return 0;
}


//checks the reaction_ensemble struct for valid parameters
int check_reaction_ensemble(){
	if(current_reaction_system.standard_pressure_in_simulation_units<0){
		printf("Please initialize your reaction ensemble standard pressure before calling initialize.\n");
		exit(0);
	}
	if(current_reaction_system.temperature_reaction_ensemble<0){
		printf("Temperatures cannot be negative. Please provide a temperature (in k_B T) to the simulation. Normally it should be 1.0. This will be used directly to calculate beta:=1/(k_B T) which occurs in the exp(-beta*E)");
		exit(0);
	}
	if(current_reaction_system.water_type>=0 &&(current_reaction_system.given_length_in_SI_units<0 || current_reaction_system.given_length_in_simulation_units <0)){
		printf("Please provide a length scale in order to make use of the water_type (e.g. for autodissociation reactions). Use the length_scales argument of the reaction_ensemble command.\n");
		exit(0);		
	}
	return 0;
}

int free_reaction_ensemble(){
	//needs to be called at the end of the simulation
	for(int single_reaction_i=0;single_reaction_i<current_reaction_system.nr_single_reactions;single_reaction_i++){
		//free educt types and coefficients
		free(current_reaction_system.reactions[single_reaction_i]->educt_types);
		free(current_reaction_system.reactions[single_reaction_i]->educt_coefficients);	
		//free product types and coefficients
		free(current_reaction_system.reactions[single_reaction_i]->product_types);
		free(current_reaction_system.reactions[single_reaction_i]->product_coefficients);
		free(current_reaction_system.reactions[single_reaction_i]);	
	}
	free(current_reaction_system.reactions);
	free(current_reaction_system.type_index);
	free(current_reaction_system.charges_of_types);

	//check wether wang_landau was used
	if(current_wang_landau_system.histogram != NULL)
		free_wang_landau();

	return 0;
}


//boring helper functions

float factorial_Ni0_divided_by_factorial_Ni0_plus_nu_i(int Ni0, int nu_i) {
	float value=1;
	
	if (nu_i == 0) {
		value=1.0;
	} else {
		value=1.0;
		if (nu_i > 0) {
			for(int i=1;i <= nu_i;i++) {
				value=value*1.0/(Ni0+i);
			}
		} else {
			int abs_nu_i =-1.0*nu_i;
			for(int i=0;i < abs_nu_i;i++) {
				value= value*(Ni0-i);
			}
		}
	}
	return value;
}


bool all_educt_particles_exist(int reaction_id) {
	bool enough_particles=true;
	for(int i=0;i<current_reaction_system.reactions[reaction_id]->len_educt_types;i++){
		int current_number;
		number_of_particles_with_type(current_reaction_system.reactions[reaction_id]->educt_types[i], &current_number);
		if(current_number<current_reaction_system.reactions[reaction_id]->educt_coefficients[i]){
			if(current_reaction_system.reactions[reaction_id]->educt_types[i]!=current_reaction_system.water_type) {
				enough_particles=false;
				break;
			} else{
				//if the current educt type is the water type, create the water molecules that are needed
				for(int water_i=0; water_i<current_reaction_system.reactions[reaction_id]->educt_coefficients[i];water_i++){
					create_particle(current_reaction_system.water_type);
				}
			}
		}
	}
	return enough_particles;
}

double calculate_current_potential_energy_of_system(int unimportant_int){
	//calculate potential energy
	//if (total_energy.init_status == 0) {
		init_energies(&total_energy);
		master_energy_calc();
	//}
  	int num_energies=total_energy.data.n;
	double kinetic_energy =total_energy.data.e[0];
	double sum_all_energies=0;
	for(int i=0;i<num_energies;i++){
		sum_all_energies+= total_energy.data.e[i];
	}
	for (int i = 0; i < n_external_potentials; i++) {
        	sum_all_energies += external_potentials[i].energy;
        }

	return sum_all_energies-kinetic_energy;
}

int generic_oneway_reaction(int reaction_id){
	float volume = box_l[0]*box_l[1]*box_l[2];
	single_reaction* current_reaction=current_reaction_system.reactions[reaction_id];
	
	//generic one way reaction
	//A+B+...+G +... --> K+...X + Z +...
	//you need to use 2A --> B instead of A+A --> B since in the last case you assume distinctness of the particles
	//further it is crucial for the function in which order you provide the educt and product types since particles will be replaced correspondingly!
	
	if (all_educt_particles_exist(reaction_id) ==false ) {
		//makes sure, no incomplete reaction is performed -> only need to consider rollback of complete reactions
		return 0;
	}
	
	//calculate potential energy
	double E_pot_old=calculate_current_potential_energy_of_system(0); //only consider potential energy since we assume that the kinetic part drops out in the process of calculating ensemble averages (kinetic part may be seperated and crossed out)
	
	//find reacting molecules in educts and save their properties for later recreation if step is not accepted
	//do reaction
	//save old particle_numbers
	int* old_particle_numbers=(int*) calloc(1,sizeof(int) *current_reaction_system.nr_different_types);
	for(int type_index=0;type_index<current_reaction_system.nr_different_types;type_index++){
		number_of_particles_with_type(current_reaction_system.type_index[type_index], &(old_particle_numbers[type_index])); // here could be optimized by not going over all types but only the types that occur in the reaction
	}
	if(current_reaction_system.water_type>=0){
		//set number of water molecules to typical value 55.5 mol/l
		//see https://de.wikipedia.org/wiki/Eigenschaften_des_Wassers#Ionenprodukt
		int index_of_water_type=find_index_of_type(current_reaction_system.water_type);
		double molar_concentration_of_water=55.5; //in mol per liter
		old_particle_numbers[index_of_water_type]=int(conversion_factor_molar_concentration_to_number_concentration_per_simulation_box_volume(current_reaction_system.given_length_in_simulation_units, current_reaction_system.given_length_in_SI_units) *molar_concentration_of_water*volume);
	}
		
	int* p_ids_created_particles =NULL;
	int len_p_ids_created_particles=0;
	double* hidden_particles_properties=NULL;
	int len_hidden_particles_properties=0;
	double* changed_particles_properties=NULL;
	int len_changed_particles_properties=0;
	int number_of_saved_properties=3; //save p_id, charge and type of the educt particle, only thing we need to hide the particle and recover it
	
	//create or hide particles of types with corresponding types in reaction
	for(int i=0;i<min(current_reaction->len_product_types,current_reaction->len_educt_types);i++){
		//change min(educt_coefficients(i),product_coefficients(i)) many particles of educt_types(i) to product_types(i)
		for(int j=0;j<min(current_reaction->product_coefficients[i],current_reaction->educt_coefficients[i]);j++){
			int p_id ;
			find_particle_type(current_reaction->educt_types[i], &p_id);
			changed_particles_properties=(double*) realloc(changed_particles_properties,sizeof(double)*(len_changed_particles_properties+1)*number_of_saved_properties);
			changed_particles_properties[len_changed_particles_properties*number_of_saved_properties]=(double) p_id;
			changed_particles_properties[len_changed_particles_properties*number_of_saved_properties+1]= current_reaction_system.charges_of_types[find_index_of_type(current_reaction->educt_types[i])];
			changed_particles_properties[len_changed_particles_properties*number_of_saved_properties+2]=(double) current_reaction->educt_types[i];
			len_changed_particles_properties+=1;
			replace(p_id,current_reaction->product_types[i]);
		}
		//create product_coefficients(i)-educt_coefficients(i) many product particles iff product_coefficients(i)-educt_coefficients(i)>0,
		//iff product_coefficients(i)-educt_coefficients(i)<0, hide this number of educt particles
		if ( current_reaction->product_coefficients[i]-current_reaction->educt_coefficients[i] >0) {
			for(int j=0; j< current_reaction->product_coefficients[i]-current_reaction->educt_coefficients[i] ;j++) {
				int p_id=-1;
				while(p_id == -1) {
					p_id=create_particle(current_reaction->product_types[i]);
				}
				p_ids_created_particles=(int*) realloc(p_ids_created_particles,sizeof(int)*(len_p_ids_created_particles+1));
				p_ids_created_particles[len_p_ids_created_particles]=p_id;
				len_p_ids_created_particles+=1;
			}
		} else if (current_reaction->educt_coefficients[i]-current_reaction->product_coefficients[i] >0) {
			for(int j=0;j<current_reaction->educt_coefficients[i]-current_reaction->product_coefficients[i];j++) {
				int p_id ;
				find_particle_type(current_reaction->educt_types[i], &p_id);
				hidden_particles_properties=(double*) realloc(hidden_particles_properties,sizeof(double)*(len_hidden_particles_properties+1)*number_of_saved_properties);
				hidden_particles_properties[len_hidden_particles_properties*number_of_saved_properties]=(double) p_id;
				hidden_particles_properties[len_hidden_particles_properties*number_of_saved_properties+1]= current_reaction_system.charges_of_types[find_index_of_type(current_reaction->educt_types[i])];
				hidden_particles_properties[len_hidden_particles_properties*number_of_saved_properties+2]=(double) current_reaction->educt_types[i];
				len_hidden_particles_properties+=1;
				hide_particle(p_id,current_reaction->educt_types[i]);
			}
		}

	}

	//create or hide particles of types with noncorresponding replacement types
	for(int i=min(current_reaction->len_product_types,current_reaction->len_educt_types);i< max(current_reaction->len_product_types,current_reaction->len_educt_types);i++ ) {
		if(current_reaction->len_product_types<current_reaction->len_educt_types){
			//hide superfluous educt_types particles
			for(int j=0;j<current_reaction->educt_coefficients[i];j++){
				int p_id ;
				find_particle_type(current_reaction->educt_types[i], &p_id);
				hidden_particles_properties=(double*) realloc(hidden_particles_properties,sizeof(double)*(len_hidden_particles_properties+1)*number_of_saved_properties);
				hidden_particles_properties[len_hidden_particles_properties*number_of_saved_properties]=(double) p_id;
				hidden_particles_properties[len_hidden_particles_properties*number_of_saved_properties+1]= current_reaction_system.charges_of_types[find_index_of_type(current_reaction->educt_types[i])];
				hidden_particles_properties[len_hidden_particles_properties*number_of_saved_properties+2]=(double) current_reaction->educt_types[i];
				len_hidden_particles_properties+=1;
				hide_particle(p_id,current_reaction->educt_types[i]);
			}
		} else {
			//create additional product_types particles
			for(int j=0;j<current_reaction->product_coefficients[i];j++){
				int p_id = -1;
				while (p_id == -1) {
					p_id= create_particle(current_reaction->product_types[i]);
				}
				p_ids_created_particles=(int*) realloc(p_ids_created_particles,sizeof(int)*(len_p_ids_created_particles+1));
				p_ids_created_particles[len_p_ids_created_particles]=p_id;
				len_p_ids_created_particles+=1;
			}
		}
	}
	
	double E_pot_new=calculate_current_potential_energy_of_system(0);
	
	double factorial_expr=1.0;
	//factorial contribution of educts
	for(int i=0;i<current_reaction->len_educt_types;i++) {
		int nu_i=-1*current_reaction->educt_coefficients[i];
		int N_i0= old_particle_numbers[find_index_of_type(current_reaction->educt_types[i])];
		factorial_expr=factorial_expr*factorial_Ni0_divided_by_factorial_Ni0_plus_nu_i(N_i0,nu_i); //zeta = 1 (see smith paper) since we only perform one reaction at one call of the function
	}
	//factorial contribution of products
	for(int i=0;i<current_reaction->len_product_types;i++) {
		int nu_i=current_reaction->product_coefficients[i];
		int N_i0= old_particle_numbers[find_index_of_type(current_reaction->product_types[i])];
		factorial_expr=factorial_expr*factorial_Ni0_divided_by_factorial_Ni0_plus_nu_i(N_i0,nu_i); //zeta = 1 (see smith paper) since we only perform one reaction at one call of the function
	}

	double beta =1.0/current_reaction_system.temperature_reaction_ensemble;
	double standard_pressure_in_simulation_units=current_reaction_system.standard_pressure_in_simulation_units;
	//calculate boltzmann factor
	double bf= pow(volume*beta*standard_pressure_in_simulation_units, current_reaction->nu_bar) * current_reaction->equilibrium_constant * factorial_expr * exp(-beta * (E_pot_new - E_pot_old));
	int reaction_is_accepted=0;
	if ( d_random() < bf ) {
		//accept
		//delete hidden educt_particles (remark: dont delete changed particles)
		for(int i=0;i<len_hidden_particles_properties*number_of_saved_properties;i+=number_of_saved_properties) {
			int p_id = (int) hidden_particles_properties[i];
			delete_particle(p_id); //delete particle
		}
		reaction_is_accepted= 1;
	} else {
		//reject
		//reverse reaction
		//1) delete created product particles
		qsort(p_ids_created_particles, len_p_ids_created_particles, sizeof(int), intcmp); // needed since delete_particle changes particle p_ids. start deletion from the largest p_id onwards
		for(int i=0;i<len_p_ids_created_particles;i++){
			delete_particle(p_ids_created_particles[i]);
		}
		//2)restore previously hidden educt particles
		for(int i=0;i<len_hidden_particles_properties*number_of_saved_properties;i+=number_of_saved_properties) {
			int p_id = (int) hidden_particles_properties[i];
			double charge= hidden_particles_properties[i+1];
			int type=(int) hidden_particles_properties[i+2];
			//set charge
			set_particle_q(p_id, charge);
			//set type
			set_particle_type(p_id, type);
		}
		//2)restore previously changed educt particles
		for(int i=0;i<len_changed_particles_properties*number_of_saved_properties;i+=number_of_saved_properties) {
			int p_id = (int) changed_particles_properties[i];
			double charge= changed_particles_properties[i+1];
			int type=(int) changed_particles_properties[i+2];
			//set charge
			set_particle_q(p_id, charge);
			//set type
			set_particle_type(p_id, type);
		}
		reaction_is_accepted= 0;
	}
	//free
	free(old_particle_numbers);
	free(changed_particles_properties);
	free(p_ids_created_particles);
	free(hidden_particles_properties);
	return reaction_is_accepted;

}

int calculate_nu_bar(int* educt_coefficients, int len_educt_types,  int* product_coefficients, int len_product_types){
	//should only be used at when defining a new reaction
	int nu_bar =0;
	for(int i=0;i<len_educt_types;i++){
		nu_bar-=educt_coefficients[i];
	}
	for(int i=0;i<len_product_types;i++){
		nu_bar+=product_coefficients[i];
	}
	return nu_bar;
}


bool _type_is_in_list(int type, int* list, int len_list){
	bool is_in_list=false;
	for(int i=0;i<len_list;i++){
		if(list[i]==type){
			is_in_list=true;
			break;
		}
	}
	return is_in_list;
}

int update_type_index(int* educt_types, int len_educt_types, int* product_types, int len_product_types){
	//should only be used at when defining a new reaction
	
	if(current_reaction_system.type_index==NULL){
		current_reaction_system.type_index=(int*) calloc(1,sizeof(int));
		current_reaction_system.type_index[0]=educt_types[0];
		current_reaction_system.nr_different_types=1;
		init_type_array(educt_types[0]); //make types known in espresso
	}
	for (int i =0; i<len_educt_types;i++){
		bool educt_type_i_is_known=_type_is_in_list(educt_types[i],current_reaction_system.type_index,current_reaction_system.nr_different_types);
		if (educt_type_i_is_known==false){
			current_reaction_system.type_index=(int*) realloc(current_reaction_system.type_index, sizeof(int)*(current_reaction_system.nr_different_types+1));
			current_reaction_system.type_index[current_reaction_system.nr_different_types]=educt_types[i];
			current_reaction_system.nr_different_types+=1;
			init_type_array(educt_types[i]); //make types known in espresso
		}
	}
	
	for (int i =0; i<len_product_types;i++){
		bool product_type_i_is_known=_type_is_in_list(product_types[i],current_reaction_system.type_index,current_reaction_system.nr_different_types);
		if (product_type_i_is_known==false){
			current_reaction_system.type_index=(int*) realloc(current_reaction_system.type_index, sizeof(int)*(current_reaction_system.nr_different_types+1));
			current_reaction_system.type_index[current_reaction_system.nr_different_types]=product_types[i];
			current_reaction_system.nr_different_types+=1;
			init_type_array(product_types[i]); //make types known in espresso
		}
	}

	//increase current_reaction_system.charges_of_types length
	current_reaction_system.charges_of_types =(double*) realloc(current_reaction_system.charges_of_types,sizeof(double)*current_reaction_system.nr_different_types);
	
	return 0;
}

int find_index_of_type(int type){
	int index =-100; //initialize to invalid index
	for(int i=0; i<current_reaction_system.nr_different_types;i++){
		if(type==current_reaction_system.type_index[i]){
			index=i;
			break;
		}
	}
	return index;
}

double conversion_factor_molar_concentration_to_number_concentration_per_simulation_box_volume(double given_length_in_simulation_units, double given_length_in_SI_units){
	//returns the factor to convert 1mol/l to x Particles per simulation volume
	//calculation:1mol/l=x/Vges <=> x=1mol/l*V_ges = 1mol/l *V_{ges,sim}*[V]= on_factor*V_{ges,sim}, with conversion_factor=1mol/l *[V]=1000mol/m^3*[V], and [V]=sigma^3=(bjerrum_length_real/len_sim)^3
	//returns the conversion factor for densities from mol/l to parts/vol in simulation units	
	//the real length scale has to be given in meters (SI)
	//an arbitrary example for arguments: given_length_in_simulation_units=bjerrum_length=2, given_length_in_SI_units=[expr 7.1 * pow(10, -10) ]; # (bjerrum length water room temp in 10^-10 m (Angstrom) )
	double N_Avogadro = 6.02214129*pow(10, 23); // in units 1/mol
	double conversion_factor= N_Avogadro *1000.0*pow(given_length_in_SI_units/given_length_in_simulation_units,3); //the factor 1000 comes from using liters
	return conversion_factor;
}

int replace(int p_id, int desired_type){
	int err_code_type=set_particle_type(p_id, desired_type);
	int err_code_q=set_particle_q(p_id, (double) current_reaction_system.charges_of_types[find_index_of_type(desired_type)]);
	return (err_code_q bitor err_code_type);
}


int hide_particle(int p_id, int previous_type){
	//remove_charge and put type to a non existing one --> no interactions anymore (not even bonds contribute to energy) it is as if the particle was non existing
	//set charge
	set_particle_q(p_id, 0.0);
	//set type
	int desired_type=previous_type+100;//+100 in order to assign types that are out of the "usual" range of types
	int err_code_type=set_particle_type(p_id, desired_type);
	return err_code_type;
}


//copies last particle to that position and deletes the then last one
int delete_particle (int p_id) {
	if (p_id == max_seen_particle) {
		// last particle, just delete
		remove_particle(p_id);
	} else {
		// otherwise, copy properties of last particle to particle with given pid, delete last particle
		// this avoids that the particle identities get excessive

		//read from last particle
		Particle last_particle;
		get_particle_data(max_seen_particle,&last_particle);
		//set pos
		double ppos[3];
		memmove(ppos, last_particle.r.p, 3*sizeof(double));

		//write to particle with p_id
		//set pos
		place_particle(p_id,ppos);
		//set velocities
		set_particle_v(p_id,last_particle.m.v);
		//set charge
		set_particle_q(p_id,last_particle.p.q);
		//set type
		set_particle_type(p_id,last_particle.p.type);


		p_id=max_seen_particle;
		remove_particle(p_id);
	}
	return 0;
}


int create_particle(int desired_type){
	//remark only works for cubic box
	int p_id=max_seen_particle+1;
	double pos_x;
	double pos_y;
	double pos_z;

	//create random velocity vector according to Maxwell Boltzmann distribution for components
	double vel[3];
	//we usse mass=1 for all particles, think about adapting this
	vel[0]=pow(2*PI*current_reaction_system.temperature_reaction_ensemble,-3.0/2.0)*gaussian_random()*time_step;//scale for internal use in espresso
	vel[1]=pow(2*PI*current_reaction_system.temperature_reaction_ensemble,-3.0/2.0)*gaussian_random()*time_step;//scale for internal use in espresso
	vel[2]=pow(2*PI*current_reaction_system.temperature_reaction_ensemble,-3.0/2.0)*gaussian_random()*time_step;//scale for internal use in espresso
	
	double charge= (double) current_reaction_system.charges_of_types[find_index_of_type(desired_type)];
	bool particle_inserted_too_close_to_another_one=true;
	int max_insert_tries=1000;
	int insert_tries=0;
	double min_dist=current_reaction_system.exclusion_radius; //setting of a minimal distance is allowed to avoid overlapping configurations if there is a repulsive potential. States with very high energies have a probability of almost zero and therefore do not contribute to ensemble averages.
	if(min_dist!=0){
		while(particle_inserted_too_close_to_another_one && insert_tries<max_insert_tries) {
			pos_x=box_l[0]*d_random();
			pos_y=box_l[1]*d_random();
			pos_z=box_l[2]*d_random();
			double pos_vec[3]={pos_x,pos_y,pos_z};
			place_particle(p_id,pos_vec);
			//set type
			set_particle_type(p_id, desired_type);
			//set charge
			set_particle_q(p_id, charge);
			//set velocities
			set_particle_v(p_id,vel);
			double d_min=distto(pos_vec,p_id);
			insert_tries+=1;
			if(d_min>current_reaction_system.exclusion_radius)
				particle_inserted_too_close_to_another_one=false;
		}
	}else{
		pos_x=box_l[0]*d_random();
		pos_y=box_l[1]*d_random();
		pos_z=box_l[2]*d_random();
		double pos_vec[3]={pos_x,pos_y,pos_z};
		place_particle(p_id,pos_vec);
		//set type
		set_particle_type(p_id, desired_type);	
		//set velocities
		set_particle_v(p_id,vel);
		//set charge
		set_particle_q(p_id, charge);
	}
	
	if(insert_tries>max_insert_tries){
		printf("Error: Particle not inserted\n");
		return -1;	
	}
	return p_id;
}

//the following 3 functions are directly taken from ABHmath.tcl
double vec_len(double* vec){
	double veclen=0;
	for(int i=0;i<3;i++){
		veclen+=pow(vec[i],2);	
	}
	return sqrt(veclen);
}

int vecnorm(double* vec, double desired_length){
	double veclen=vec_len(vec);
	for(int i=0;i<3;i++){
		vec[i]=vec[i]/veclen*desired_length;	
	}
	return 0;
}

int vec_random(double* vecrandom, double desired_length){
	//returns a random vector of length len
	//(uniform distribution on a sphere)
	//This is done by chosing 3 uniformly distributed random numbers [-1,1]
	//If the length of the resulting vector is <= 1.0 the vector is taken and normalized
	//to the desired length, otherwise the procedure is repeated until succes.
	//On average the procedure needs 5.739 random numbers per vector.
	//(This is probably not the most efficient way, but it works!)
	//Ask your favorit mathematician for a proof!
	while(1){
		for(int i=0;i<3;i++){
			vecrandom[i]=2*d_random()-1.0;
		}
		if (vec_len(vecrandom)<=1)
			break;
	}
	vecnorm(vecrandom,desired_length);
	return 0;
}

//for sorting integer lists, derived from http://rosettacode.org/wiki/Sort_an_integer_array#C, function is licensed under GPL
int intcmp(const void *aa, const void *bb){ 
    const int *a = (int*) aa, *b =(int*)  bb;
    return (*a > *b) ? -1 : (*a < *b);
}


///////////////////////////////////////////// Wang-Landau algorithm

int get_flattened_index_wang_landau(double* current_state, double* collective_variables_minimum_values, double* collective_variables_maximum_values, double* delta_collective_variables_values, int nr_collective_variables){
	int index=-10; //negative number is not allowed as index and therefore indicates error
	int individual_indices[nr_collective_variables]; //pre result
	memset(individual_indices, -1, sizeof(individual_indices)); //initialize individual_indices to -1
	int nr_subindices_of_collective_variable[nr_collective_variables];
	//current_state[1]=delta_collective_variables_values[1]-0.00001;

	//check for the current state to be an allowed state in the [range collective_variables_minimum_values:collective_variables_maximum_values], else return a negative index
	for(int collective_variable_i=0;collective_variable_i<nr_collective_variables;collective_variable_i++){
		if(current_state[collective_variable_i]>collective_variables_maximum_values[collective_variable_i]+delta_collective_variables_values[collective_variable_i] || current_state[collective_variable_i]<collective_variables_minimum_values[collective_variable_i])
			return index;
	}

	for(int collective_variable_i=0;collective_variable_i<nr_collective_variables;collective_variable_i++){
		nr_subindices_of_collective_variable[collective_variable_i]=int((collective_variables_maximum_values[collective_variable_i]-collective_variables_minimum_values[collective_variable_i])/delta_collective_variables_values[collective_variable_i])+1; //+1 for collecive variables which are of type degree of association
		//XXX avoid the begin and the end of the CV_interval not being a multiple of the delta_CV (e.g. for energy collective variable)
		for(int subindex_i=0;subindex_i<nr_subindices_of_collective_variable[collective_variable_i];subindex_i++){
			if( current_state[collective_variable_i]<(subindex_i+1)*delta_collective_variables_values[collective_variable_i]+collective_variables_minimum_values[collective_variable_i]-delta_collective_variables_values[collective_variable_i]/100000.0){
				//-delta_collective_variables_values[collective_variable_i]/100000 is due to numeric reasons (think of the degree of association as a collective variable)
				individual_indices[collective_variable_i]=subindex_i;
				break;
			}
		}
	}
	
	//get flattened index from individual_indices
	index=0; //this is already part of the algorithm to find the correct index
	for(int collective_variable_i=0;collective_variable_i<nr_collective_variables;collective_variable_i++){
		int factor=1;
		for(int j=collective_variable_i+1;j<nr_collective_variables;j++){
			factor*=nr_subindices_of_collective_variable[j];
		}
		index+=factor*individual_indices[collective_variable_i];
		
	}

	return index;
}

int get_flattened_index_wang_landau_of_current_state(){
	int nr_collective_variables=current_wang_landau_system.nr_collective_variables;
	//get current state
	double current_state[nr_collective_variables];
	for(int CV_i=0;CV_i<nr_collective_variables;CV_i++){
		current_state[CV_i]=current_wang_landau_system.collective_variables[CV_i]->determine_current_state_in_collective_variable_with_index(CV_i);	
	}

	//get collective_variables_minimum_values
	double collective_variables_minimum_values[nr_collective_variables];
	for(int CV_i=0;CV_i<nr_collective_variables;CV_i++){
		collective_variables_minimum_values[CV_i]=current_wang_landau_system.collective_variables[CV_i]->CV_minimum;	
	}
	//get collective_variables_maximum_values
	double collective_variables_maximum_values[nr_collective_variables];
	for(int CV_i=0;CV_i<nr_collective_variables;CV_i++){
		collective_variables_maximum_values[CV_i]=current_wang_landau_system.collective_variables[CV_i]->CV_maximum;	
	}
	//get delta_collective_variables_values
	double delta_collective_variables_values[nr_collective_variables];
	for(int CV_i=0;CV_i<nr_collective_variables;CV_i++){
		delta_collective_variables_values[CV_i]=current_wang_landau_system.collective_variables[CV_i]->delta_CV;	
	}
	int index=get_flattened_index_wang_landau(current_state, collective_variables_minimum_values, collective_variables_maximum_values, delta_collective_variables_values, nr_collective_variables);
	return index;
}


wang_landau_system current_wang_landau_system={.histogram=NULL,.len_histogram=0 , \
						.wang_landau_potential=NULL,.nr_collective_variables=0,\
						.collective_variables=NULL, .wang_landau_parameter=1.0,\
 						.initial_wang_landau_parameter=1.0,.already_refined_n_times=0, \
 						.int_fill_value=-10,.double_fill_value=-10.0,\
 						.number_of_monte_carlo_moves_between_check_of_convergence=5000, .final_wang_landau_parameter=0.00001,\
 						.monte_carlo_trial_moves=0, .wang_landau_steps=1,\
 						.output_filename=NULL,\
 						.minimum_energies_at_flat_index=NULL, .maximum_energies_at_flat_index=NULL,\
 						.do_energy_reweighting=false, .counter_ion_type=-10, .polymer_start_id=-10 ,\
 						.polymer_end_id=-10, .fix_polymer=false ,.do_not_sample_reaction_partition_function=false,\
 					.used_bins=-10
 						};//use negative value as fill value since it cannot occur in the wang_landau algorithm in the histogram and in the wang landau potential, use only 1 wang_landau_steps if you want to record other observables in the tcl script.

double get_minimum_CV_value_on_delta_CV_spaced_grid(double min_CV_value, double delta_CV) {
	//assume grid has it s origin at 0
	double minimum_CV_value_on_delta_CV_spaced_grid=floor(min_CV_value/delta_CV)*delta_CV;
	return minimum_CV_value_on_delta_CV_spaced_grid;
};


double calculate_delta_degree_of_association(int index_of_current_collective_variable){
	//calculate Delta in the degree of association so that EVERY reaction step is driven.
	collective_variable* current_collective_variable=current_wang_landau_system.collective_variables[index_of_current_collective_variable];
	int total_number_of_corresponding_acid=0;
	for(int corresponding_type_i=0; corresponding_type_i<current_collective_variable->nr_corresponding_acid_types;corresponding_type_i++){
		int num_of_current_type;
		number_of_particles_with_type(current_collective_variable->corresponding_acid_types[corresponding_type_i],&num_of_current_type);
		total_number_of_corresponding_acid+=num_of_current_type;
	}
	double delta=1.0/total_number_of_corresponding_acid;
	//now modify the minimum value of the CV to lie on the grid
	current_collective_variable->CV_minimum=get_minimum_CV_value_on_delta_CV_spaced_grid(current_collective_variable->CV_minimum,delta);
	return delta ;
}

int* initialize_histogram(){
	int needed_bins=1;
	for(int CV_i=0;CV_i<current_wang_landau_system.nr_collective_variables;CV_i++){
		collective_variable* current_collective_variable=current_wang_landau_system.collective_variables[CV_i];
		needed_bins*=int((current_collective_variable->CV_maximum-current_collective_variable->CV_minimum)/current_collective_variable->delta_CV)+1; // plus 1 needed for degrees of association related part of histogram (think of only one acid particle)
	}
	int* histogram =(int*) calloc(1,sizeof(int)*needed_bins); //calloc initializes everything to zero
	current_wang_landau_system.len_histogram=needed_bins;
	return histogram;
}

double* initialize_wang_landau_potential(){
	int needed_bins=1;
	for(int CV_i=0;CV_i<current_wang_landau_system.nr_collective_variables;CV_i++){
		collective_variable* current_collective_variable=current_wang_landau_system.collective_variables[CV_i];
		needed_bins*=int((current_collective_variable->CV_maximum-current_collective_variable->CV_minimum)/current_collective_variable->delta_CV)+1; // plus 1 needed for degrees of association related part of histogram (think of only one acid particle) 
	}
	double* wang_landau_potential =(double*) calloc(1,sizeof(double)*needed_bins); //calloc initializes everything to zero
	return wang_landau_potential;
}

double calculate_degree_of_association(int index_of_current_collective_variable){
	collective_variable* current_collective_variable=current_wang_landau_system.collective_variables[index_of_current_collective_variable];
	int total_number_of_corresponding_acid=0;
	for(int corresponding_type_i=0; corresponding_type_i<current_collective_variable->nr_corresponding_acid_types;corresponding_type_i++){
		int num_of_current_type;
		number_of_particles_with_type(current_collective_variable->corresponding_acid_types[corresponding_type_i],&num_of_current_type);
		total_number_of_corresponding_acid+=num_of_current_type;
	}
	if(total_number_of_corresponding_acid==0){
		printf("Have you forgotten to specify all corresponding acid types? Total particle number of corresponding acid type is zero\n");
		fflush(stdout);
	}
	int num_of_associated_acid;
	number_of_particles_with_type(current_collective_variable->associated_type,&num_of_associated_acid);
	double degree_of_association=(double) num_of_associated_acid/total_number_of_corresponding_acid; //cast to double because otherwise any fractional part is lost
	return degree_of_association;
}

double find_minimum_non_negative_value(double* list, int len){
	double minimum =list[0];
	for (int i=0;i<len;i++){
		if(minimum<0)
			minimum=list[i];//think of negative histogram values that indicate not allowed energies in the case of an energy observable
		if(list[i]<minimum && list[i]>=0)
			minimum=list[i];	
	}
	return minimum;
}

double find_minimum(double* list, int len){
	double minimum =list[0];
	for (int i=0;i<len;i++){
		if(list[i]<minimum)
			minimum=list[i];	
	}
	return minimum;
}

double find_maximum(double* list, int len){
	double maximum =list[0];
	for (int i=0;i<len;i++){
		if(list[i]>maximum)
			maximum=list[i];	
	}
	return maximum;
}

int get_flattened_index_wang_landau_without_energy_collective_variable(int flattened_index_with_energy_collective_variable, int collective_variable_index_energy_observable);//needed for energy collective variable
void unravel_index(int* len_dims, int ndims, int flattened_index, int* unraveled_index_out); //needed for writing results and energy collective variable

int initialize_wang_landau(){
	//initialize deltas for collective variables which are of the type of a degree of association
	int energy_collective_variable_index=-10;
	double* min_boundaries_energies=NULL;
	double* max_boundaries_energies=NULL;
	for(int collective_variable_i=0; collective_variable_i<current_wang_landau_system.nr_collective_variables;collective_variable_i++){
		collective_variable* current_collective_variable=current_wang_landau_system.collective_variables[collective_variable_i];
		if(current_collective_variable->corresponding_acid_types!=NULL){
			//found a collective variable which is of the type of a degree_of_association
			current_collective_variable->delta_CV=calculate_delta_degree_of_association(collective_variable_i);
		}
		
		int flattened_index_previous_run=0; //len_histogram of energy preparation run
		if(current_collective_variable->energy_boundaries_filename!=NULL){
			//found a collective variable which is not of the type of an energy
			current_wang_landau_system.do_energy_reweighting=true;
			energy_collective_variable_index=collective_variable_i;
			//load energy boundaries from file
			FILE* pFile;
			pFile = fopen(current_collective_variable->energy_boundaries_filename,"r");
			if (pFile==NULL){
				printf("ERROR: energy boundaries file for the specific system could not be read.\n");
				exit(0);
				// Note that you cannot change the other collective variables in the pre-production run and the production run
				return false;
			}
			//save minimum and maximum energies as a function of the other collective variables under current_wang_landau_system.energ...
			char *line = NULL;
			size_t len = 0;
			ssize_t length_line;
			const char* delim="\t ";
			getline(&line, &len, pFile);//dummy call of getline to get rid of header line (first line in file)
			while ((length_line = getline(&line, &len, pFile)) != -1) {
				int counter_words_in_line=0;
				for(char* word=strtok(line,delim);word!=NULL;word=strtok(NULL,delim)){
					if(counter_words_in_line<current_wang_landau_system.nr_collective_variables-1){
						counter_words_in_line+=1;
						continue;
					}else if(counter_words_in_line==current_wang_landau_system.nr_collective_variables-1){
						double energy_boundary_minimum=atof(word);
						counter_words_in_line+=1;
						min_boundaries_energies=(double*) realloc(min_boundaries_energies,sizeof(double)*(flattened_index_previous_run+1));			
						min_boundaries_energies[flattened_index_previous_run]=energy_boundary_minimum;
					}else if(counter_words_in_line==current_wang_landau_system.nr_collective_variables){
						double energy_boundary_maximum=atof(word);
						max_boundaries_energies=(double*) realloc(max_boundaries_energies,sizeof(double)*(flattened_index_previous_run+1));				
						max_boundaries_energies[flattened_index_previous_run]=energy_boundary_maximum;
						counter_words_in_line+=1;							
					}
					
				}
				flattened_index_previous_run+=1;		
			}
			
			current_collective_variable->CV_minimum=find_minimum(min_boundaries_energies,flattened_index_previous_run);
			current_collective_variable->CV_maximum=find_maximum(max_boundaries_energies,flattened_index_previous_run);
		}
	}


	//construct (possibly higher dimensional) histogram over Gamma (the room which should be equally sampled when the wang-landau algorithm has converged)
	current_wang_landau_system.histogram=initialize_histogram();

	//construct (possibly higher dimensional) wang_landau potential over Gamma (the room which should be equally sampled when the wang-landau algorithm has converged)
	current_wang_landau_system.wang_landau_potential=initialize_wang_landau_potential();
	
	current_wang_landau_system.used_bins=current_wang_landau_system.len_histogram; //initialize for 1/t wang_landau algorithm
	
	if(energy_collective_variable_index>=0){
		//make values in histogram and wang landau potential negative if they are not allowed at the given degree of association, because the energy boundaries prohibit them

		int nr_subindices_of_collective_variable[current_wang_landau_system.nr_collective_variables];
		for(int collective_variable_i=0;collective_variable_i<current_wang_landau_system.nr_collective_variables;collective_variable_i++){
			nr_subindices_of_collective_variable[collective_variable_i]=int((current_wang_landau_system.collective_variables[collective_variable_i]->CV_maximum-current_wang_landau_system.collective_variables[collective_variable_i]->CV_minimum)/current_wang_landau_system.collective_variables[collective_variable_i]->delta_CV)+1; //+1 for collecive variables which are of type degree of association
		}

		int empty_bins_in_memory=0;

		for(int flattened_index=0;flattened_index<current_wang_landau_system.len_histogram;flattened_index++){
			//unravel index
			int unraveled_index[current_wang_landau_system.nr_collective_variables];
			unravel_index(nr_subindices_of_collective_variable,current_wang_landau_system.nr_collective_variables,flattened_index,unraveled_index);
			//use unraveled index
			double current_energy=unraveled_index[energy_collective_variable_index]*current_wang_landau_system.collective_variables[energy_collective_variable_index]->delta_CV+current_wang_landau_system.collective_variables[energy_collective_variable_index]->CV_minimum;
			if(current_energy>max_boundaries_energies[get_flattened_index_wang_landau_without_energy_collective_variable(flattened_index,energy_collective_variable_index)] || current_energy<min_boundaries_energies[get_flattened_index_wang_landau_without_energy_collective_variable(flattened_index,energy_collective_variable_index)] || max_boundaries_energies[get_flattened_index_wang_landau_without_energy_collective_variable(flattened_index,energy_collective_variable_index)]-min_boundaries_energies[get_flattened_index_wang_landau_without_energy_collective_variable(flattened_index,energy_collective_variable_index)]<0.00001 ){
				current_wang_landau_system.histogram[flattened_index]=current_wang_landau_system.int_fill_value;
				current_wang_landau_system.wang_landau_potential[flattened_index]=current_wang_landau_system.double_fill_value;
				empty_bins_in_memory+=1;		
			}
		}
		
		current_wang_landau_system.used_bins=current_wang_landau_system.len_histogram-empty_bins_in_memory;
		
	}

	//assign determine_current_state_in_this_collective_variable function pointers to correct function
	for(int collective_variable_i=0; collective_variable_i<current_wang_landau_system.nr_collective_variables;collective_variable_i++){
		collective_variable* current_collective_variable=current_wang_landau_system.collective_variables[collective_variable_i];
		if(current_collective_variable->corresponding_acid_types!=NULL){
			//found a collective variable which is not of the type of a degree_of_association association)	
			current_collective_variable->determine_current_state_in_collective_variable_with_index=&calculate_degree_of_association;
		}
		if(current_collective_variable->energy_boundaries_filename!=NULL){
			//found a collective variable which is not of the type of an energy
			current_collective_variable->determine_current_state_in_collective_variable_with_index=&calculate_current_potential_energy_of_system;
		}
		
	}
	
	//set automatically molecular dynamics time step in a good manner for the Wang-Landau reaction ensemble that for Hybrid Monte Carlo moves
	
	
	return true;
}

//derived from 	generic_oneway_reaction()
int generic_oneway_reaction_wang_landau(int reaction_id){
	float volume = box_l[0]*box_l[1]*box_l[2]; //since espresso uses cuboid boxes
	single_reaction* current_reaction=current_reaction_system.reactions[reaction_id];
	
	int old_state_index=get_flattened_index_wang_landau_of_current_state();
	if(old_state_index>=0){
		if(current_wang_landau_system.histogram[old_state_index]>=0)
			current_wang_landau_system.monte_carlo_trial_moves+=1;
	}

	//generic one way reaction
	//A+B+...+G +... --> K+...X + Z +...
	//you need to use 2A --> B instead of A+A --> B since in the last case you assume distinctness of the particles
	//further it is crucial for the function in which order you provide the educt and product types since particles will be replaced correspondingly!
	if (all_educt_particles_exist(reaction_id) ==false ) {
		//makes sure, no incomplete reaction is performed -> only need to consider rollback of complete reactions

		//increase the wang landau potential and histogram at the current nbar (this case covers the cases nbar=0 or nbar=1)
		if(old_state_index>=0 ){
			if(current_wang_landau_system.histogram[old_state_index]>=0){
				current_wang_landau_system.histogram[old_state_index]+=1;
				current_wang_landau_system.wang_landau_potential[old_state_index]+=current_wang_landau_system.wang_landau_parameter;
			}
		}
		
		return 0;
	}
	
	
	//calculate potential energy
	double E_pot_old=calculate_current_potential_energy_of_system(0); //only consider potential energy since we assume that the kinetic part drops out in the process of calculating ensemble averages (kinetic part may be seperated and crossed out)
	
	//find reacting molecules in educts and save their properties for later recreation if step is not accepted
	//do reaction
	//save old particle_numbers
	int* old_particle_numbers=(int*) calloc(1,sizeof(int) *current_reaction_system.nr_different_types);
	for(int type_index=0;type_index<current_reaction_system.nr_different_types;type_index++){
		number_of_particles_with_type(current_reaction_system.type_index[type_index], &(old_particle_numbers[type_index])); // here could be optimized by not going over all types but only the types that occur in the reaction
	}
	if(current_reaction_system.water_type>=0){
		//set number of water molecules to typical value 55.5 mol/l
		//see https://de.wikipedia.org/wiki/Eigenschaften_des_Wassers#Ionenprodukt
		int index_of_water_type=find_index_of_type(current_reaction_system.water_type);
		old_particle_numbers[index_of_water_type]=int(conversion_factor_molar_concentration_to_number_concentration_per_simulation_box_volume(current_reaction_system.given_length_in_simulation_units, current_reaction_system.given_length_in_SI_units) *55.5*volume);
	}
	int* p_ids_created_particles =NULL;
	int len_p_ids_created_particles=0;
	double* hidden_particles_properties=NULL;
	int len_hidden_particles_properties=0;
	double* changed_particles_properties=NULL;
	int len_changed_particles_properties=0;
	int number_of_saved_properties=3; //save p_id, charge and type of the educt particle, only thing we need to hide the particle and recover it
	
	//create or hide particles of types with corresponding types in reaction
	for(int i=0;i<min(current_reaction->len_product_types,current_reaction->len_educt_types);i++){
		//change min(educt_coefficients(i),product_coefficients(i)) many particles of educt_types(i) to product_types(i)
		for(int j=0;j<min(current_reaction->product_coefficients[i],current_reaction->educt_coefficients[i]);j++){
			int p_id ;
			find_particle_type(current_reaction->educt_types[i], &p_id);
			changed_particles_properties=(double*) realloc(changed_particles_properties,sizeof(double)*(len_changed_particles_properties+1)*number_of_saved_properties);
			changed_particles_properties[len_changed_particles_properties*number_of_saved_properties]=(double) p_id;
			changed_particles_properties[len_changed_particles_properties*number_of_saved_properties+1]= current_reaction_system.charges_of_types[find_index_of_type(current_reaction->educt_types[i])];
			changed_particles_properties[len_changed_particles_properties*number_of_saved_properties+2]=(double) current_reaction->educt_types[i];
			len_changed_particles_properties+=1;
			replace(p_id,current_reaction->product_types[i]);
		}
		//create product_coefficients(i)-educt_coefficients(i) many product particles iff product_coefficients(i)-educt_coefficients(i)>0,
		//iff product_coefficients(i)-educt_coefficients(i)<0, hide this number of educt particles
		if ( current_reaction->product_coefficients[i]-current_reaction->educt_coefficients[i] >0) {
			for(int j=0; j< current_reaction->product_coefficients[i]-current_reaction->educt_coefficients[i] ;j++) {
				int p_id=-1;
				while(p_id == -1) {
					p_id=create_particle(current_reaction->product_types[i]);
				}
				p_ids_created_particles=(int*) realloc(p_ids_created_particles,sizeof(int)*(len_p_ids_created_particles+1));
				p_ids_created_particles[len_p_ids_created_particles]=p_id;
				len_p_ids_created_particles+=1;
			}
		} else if (current_reaction->educt_coefficients[i]-current_reaction->product_coefficients[i] >0) {
			for(int j=0;j<current_reaction->educt_coefficients[i]-current_reaction->product_coefficients[i];j++) {
				int p_id ;
				find_particle_type(current_reaction->educt_types[i], &p_id);
				hidden_particles_properties=(double*) realloc(hidden_particles_properties,sizeof(double)*(len_hidden_particles_properties+1)*number_of_saved_properties);
				hidden_particles_properties[len_hidden_particles_properties*number_of_saved_properties]=(double) p_id;
				hidden_particles_properties[len_hidden_particles_properties*number_of_saved_properties+1]= (double) current_reaction_system.charges_of_types[find_index_of_type(current_reaction->educt_types[i])];
				hidden_particles_properties[len_hidden_particles_properties*number_of_saved_properties+2]=(double) current_reaction->educt_types[i];
				len_hidden_particles_properties+=1;
				hide_particle(p_id,current_reaction->educt_types[i]);
			}
		}

	}
	//create or hide particles of types with noncorresponding replacement types
	for(int i=min(current_reaction->len_product_types,current_reaction->len_educt_types);i< max(current_reaction->len_product_types,current_reaction->len_educt_types);i++ ) {
		if(current_reaction->len_product_types<current_reaction->len_educt_types){
			//hide superfluous educt_types particles
			for(int j=0;j<current_reaction->educt_coefficients[i];j++){
				int p_id ;
				find_particle_type(current_reaction->educt_types[i], &p_id);
				hidden_particles_properties=(double*) realloc(hidden_particles_properties,sizeof(double)*(len_hidden_particles_properties+1)*number_of_saved_properties);
				hidden_particles_properties[len_hidden_particles_properties*number_of_saved_properties]=(double) p_id;
				hidden_particles_properties[len_hidden_particles_properties*number_of_saved_properties+1]= (double) current_reaction_system.charges_of_types[find_index_of_type(current_reaction->educt_types[i])];
				hidden_particles_properties[len_hidden_particles_properties*number_of_saved_properties+2]=(double) current_reaction->educt_types[i];
				len_hidden_particles_properties+=1;
				hide_particle(p_id,current_reaction->educt_types[i]);
			}
		} else {
			//create additional product_types particles
			for(int j=0;j<current_reaction->product_coefficients[i];j++){
				int p_id = -1;
				while (p_id == -1) {
					p_id= create_particle(current_reaction->product_types[i]);
				}
				p_ids_created_particles=(int*) realloc(p_ids_created_particles,sizeof(int)*(len_p_ids_created_particles+1));
				p_ids_created_particles[len_p_ids_created_particles]=p_id;
				len_p_ids_created_particles+=1;
			}
		}
	}	
	
	double E_pot_new=calculate_current_potential_energy_of_system(0);
	//save new_state_index
	int new_state_index=get_flattened_index_wang_landau_of_current_state();
	
	double factorial_expr=1.0;
	//factorial contribution of educts
	for(int i=0;i<current_reaction->len_educt_types;i++) {
		int nu_i=-1*current_reaction->educt_coefficients[i];
		int N_i0= old_particle_numbers[find_index_of_type(current_reaction->educt_types[i])];
		factorial_expr=factorial_expr*factorial_Ni0_divided_by_factorial_Ni0_plus_nu_i(N_i0,nu_i); //zeta = 1 (see smith paper) since we only perform one reaction at one call of the function
	}
	//factorial contribution of products
	for(int i=0;i<current_reaction->len_product_types;i++) {
		int nu_i=current_reaction->product_coefficients[i];
		int N_i0= old_particle_numbers[find_index_of_type(current_reaction->product_types[i])];
		factorial_expr=factorial_expr*factorial_Ni0_divided_by_factorial_Ni0_plus_nu_i(N_i0,nu_i); //zeta = 1 (see smith paper) since we only perform one reaction at one call of the function
	}
	double beta =1.0/current_reaction_system.temperature_reaction_ensemble;
	double standard_pressure_in_simulation_units=current_reaction_system.standard_pressure_in_simulation_units;
	
	
	//determine the acceptance probabilities of the reaction move
	double bf;
	if(current_wang_landau_system.do_not_sample_reaction_partition_function==true){
		bf=1.0;
	}else{
		bf=pow(volume*beta*standard_pressure_in_simulation_units, current_reaction->nu_bar) * current_reaction->equilibrium_constant * factorial_expr;
	}
	
	if(current_wang_landau_system.do_energy_reweighting==false){
		bf= bf * exp(-beta * (E_pot_new - E_pot_old));
	}else{
		//pass
	}
	
	
	
	//look wether the proposed state lies in Gamma and add the Wang-Landau modification factor, this is a bit nasty due to the energy collective variable case (memory layout of storage array of the histogram and the wang_landau_potential values is "cuboid")
	if(old_state_index>=0 && new_state_index>=0){
		if(current_wang_landau_system.histogram[new_state_index]>=0 &&current_wang_landau_system.histogram[old_state_index]>=0 ){
			bf=min(1.0, bf*exp(current_wang_landau_system.wang_landau_potential[old_state_index]-current_wang_landau_system.wang_landau_potential[new_state_index])); //modify boltzmann factor according to wang-landau algorithm, according to grand canonical simulation paper "Density-of-states Monte Carlo method for simulation of fluids"
			//this makes the new state being accepted with the conditinal probability bf (bf is a transition probability = conditional probability from the old state to move to the new state)
		}else{
			if(current_wang_landau_system.histogram[new_state_index]>=0 &&current_wang_landau_system.histogram[old_state_index]<0 )
				bf=10;//this makes the reaction get accepted, since we found a state in Gamma
			else if (current_wang_landau_system.histogram[new_state_index]<0 &&current_wang_landau_system.histogram[old_state_index]<0)
				bf=10;//accept, in order to be able to sample new configs, which might lie in Gamma
			else if(current_wang_landau_system.histogram[new_state_index]<0 &&current_wang_landau_system.histogram[old_state_index]>=0)
				bf=-10;//this makes the reaction get rejected, since the new state is not in Gamma while the old sate was in Gamma
		}
		
	}else if(old_state_index<0 && new_state_index>=0){
		bf=10;	//this makes the reaction get accepted, since we found a state in Gamma
	}else if(old_state_index<0 && new_state_index<0){
		bf=10;	//accept, in order to be able to sample new configs, which might lie in Gamma
	}else if(old_state_index>=0 && new_state_index<0){
		bf=-10; //this makes the reaction get rejected, since the new state is not in Gamma while the old sate was in Gamma
	}
	
	int reaction_is_accepted=0;
	if ( d_random() < bf ) {
		//accept
		if(new_state_index>=0 ){
			if(current_wang_landau_system.histogram[new_state_index]>=0){
				current_wang_landau_system.histogram[new_state_index]+=1;
				current_wang_landau_system.wang_landau_potential[new_state_index]+=current_wang_landau_system.wang_landau_parameter;
			}
		}

		//delete hidden educt_particles (remark: dont delete changed particles)
		for(int i=0;i<len_hidden_particles_properties*number_of_saved_properties;i+=number_of_saved_properties) {
			int p_id = (int) hidden_particles_properties[i];
			delete_particle(p_id); //delete particle
		}
		reaction_is_accepted= 1;
	} else {
		if(old_state_index>=0){
			if(current_wang_landau_system.histogram[old_state_index]>=0){
				current_wang_landau_system.histogram[old_state_index]+=1;
				current_wang_landau_system.wang_landau_potential[old_state_index]+=current_wang_landau_system.wang_landau_parameter;
			}
		}
		//reverse reaction
		//1) delete created product particles
		qsort(p_ids_created_particles, len_p_ids_created_particles, sizeof(int), intcmp); // needed since delete_particle changes particle p_ids. start deletion from the largest p_id onwards
		for(int i=0;i<len_p_ids_created_particles;i++){
			delete_particle(p_ids_created_particles[i]);
		}
		//2)restore previously hidden educt particles
		for(int i=0;i<len_hidden_particles_properties*number_of_saved_properties;i+=number_of_saved_properties) {
			int p_id = (int) hidden_particles_properties[i];
			double charge=hidden_particles_properties[i+1];
			int type=(int) hidden_particles_properties[i+2];
			//set charge
			set_particle_q(p_id, charge);
			//set type
			set_particle_type(p_id, type);
		}
		//2)restore previously changed educt particles
		for(int i=0;i<len_changed_particles_properties*number_of_saved_properties;i+=number_of_saved_properties) {
			int p_id = (int) changed_particles_properties[i];
			double charge= changed_particles_properties[i+1];
			int type=(int) changed_particles_properties[i+2];
			//set charge
			set_particle_q(p_id, charge);
			//set type
			set_particle_type(p_id, type);
		}
		reaction_is_accepted= 0;
	}

	//free
	free(old_particle_numbers);
	free(changed_particles_properties);
	free(p_ids_created_particles);
	free(hidden_particles_properties);
	return reaction_is_accepted;
}

//declarations
void write_wang_landau_results_to_file(char* full_path_to_output_filename); 
bool can_refine_wang_landau_one_over_t();
bool achieved_desired_number_of_refinements_one_over_t ();
void refine_wang_landau_parameter_one_over_t();

bool is_in_list(int value, int* list, int len_list){
	if(len_list==0){
		return true;
	}
	for(int i=0;i<len_list;i++){
		if(list[i]==value)
			return true;
	}
	return false;
	
}

bool do_global_mc_move_for_type(int type, int start_id_polymer, int end_id_polymer){
	current_wang_landau_system.monte_carlo_trial_moves+=1;
	int p_id;
	Particle part;
	
	int old_state_index=get_flattened_index_wang_landau_of_current_state();
	double E_pot_old=calculate_current_potential_energy_of_system(0);
	
	
	int particle_number_of_type;
	number_of_particles_with_type(type, &(particle_number_of_type));
	if(particle_number_of_type==0){
		bool got_accepted=false;
		//reject
		if(old_state_index>=0){
			if(current_wang_landau_system.histogram[old_state_index]>=0){
				current_wang_landau_system.histogram[old_state_index]+=1;
				current_wang_landau_system.wang_landau_potential[old_state_index]+=current_wang_landau_system.wang_landau_parameter;
			}
		}
		return got_accepted;
	}

	double particle_positions[3*particle_number_of_type];
	int changed_particle_counter=0;
	int p_id_s_changed_particles[particle_number_of_type];
	
	//save old_position
	while(changed_particle_counter<particle_number_of_type){
		if(changed_particle_counter==0){
			find_particle_type(type, &p_id);
		}else{
			//determine a p_id you have not touched yet
			while(is_in_list(p_id,p_id_s_changed_particles,changed_particle_counter)){
				find_particle_type(type, &p_id); //check wether you already touched this p_id
			}
		}
		
		get_particle_data(p_id, &part);
		double ppos[3];
		memmove(ppos, part.r.p, 3*sizeof(double));
		particle_positions[3*changed_particle_counter]=ppos[0];
		particle_positions[3*changed_particle_counter+1]=ppos[1];
		particle_positions[3*changed_particle_counter+2]=ppos[2];
		double random_position=box_l[1]*d_random();
		double temp_pos[3]={random_position,random_position,random_position}; //move ions out of way so that they don't hinder you creating a completely new configuration
		place_particle(p_id,temp_pos);
		p_id_s_changed_particles[changed_particle_counter]=p_id;
		changed_particle_counter+=1;
	}
	
	//propose new positions
	changed_particle_counter=0;
	int max_tries=100*particle_number_of_type;//important for very dense systems
	int attempts=0;
	while(changed_particle_counter<particle_number_of_type){
		p_id=p_id_s_changed_particles[changed_particle_counter];
		bool particle_inserted_too_close_to_another_one=true;
		while(particle_inserted_too_close_to_another_one==true&& attempts<max_tries){
			//change particle position
			double pos_x=box_l[0]*bit_random_generator(); //here we use the r250 random generator with higher period by using box_l[0]*bit_random_generator()
			double pos_y=box_l[1]*d_random();
			double pos_z=box_l[2]*d_random();
			double new_pos[3]={pos_x, pos_y, pos_z};
			place_particle(p_id,new_pos);
			double d_min=distto(new_pos,p_id);
			if(d_min>current_reaction_system.exclusion_radius){
				particle_inserted_too_close_to_another_one=false;
			}
			attempts+=1;
		}
		changed_particle_counter+=1;
	}
	if(attempts==max_tries){
		//reversing
		//create particles again at the positions they were
		for(int i=0;i<particle_number_of_type;i++){
			double pos_x=particle_positions[3*i];
			double pos_y=particle_positions[3*i+1];
			double pos_z=particle_positions[3*i+2];
			double pos_vec[3]={pos_x,pos_y,pos_z};
			place_particle(p_id_s_changed_particles[i],pos_vec);
		}
	}
	
	
	//change polymer conformation if start and end id are provided
	double old_pos_polymer_particle[3*(end_id_polymer-start_id_polymer+1)];
	if((start_id_polymer!=current_wang_landau_system.int_fill_value && end_id_polymer !=current_wang_landau_system.int_fill_value) && current_wang_landau_system.fix_polymer==false){
		
		Particle part;
		for(int i=start_id_polymer;i<=end_id_polymer;i++){
			get_particle_data(i, &part);
			double ppos[3];
			memmove(ppos, part.r.p, 3*sizeof(double));
			old_pos_polymer_particle[3*i]=ppos[0];
			old_pos_polymer_particle[3*i+1]=ppos[1];
			old_pos_polymer_particle[3*i+2]=ppos[2];
			//move particle to new position nearby
			double random_direction_vector[3];
			double length_of_displacement=0.05;
			vec_random(random_direction_vector, length_of_displacement);
			double pos_x=old_pos_polymer_particle[3*i]+random_direction_vector[0];
			double pos_y=old_pos_polymer_particle[3*i+1]+random_direction_vector[1];
			double pos_z=old_pos_polymer_particle[3*i+2]+random_direction_vector[2];
			double new_pos[3]={pos_x, pos_y, pos_z};
			place_particle(i,new_pos);
		}
		
	}
	
	int new_state_index=get_flattened_index_wang_landau_of_current_state();
	
	double E_pot_new=calculate_current_potential_energy_of_system(0);
	double beta =1.0/current_reaction_system.temperature_reaction_ensemble;
	
	double bf=1.0;
	if(old_state_index>=0 && new_state_index>=0){
		if(current_wang_landau_system.histogram[new_state_index]>=0 &&current_wang_landau_system.histogram[old_state_index]>=0 ){
			if(current_wang_landau_system.do_energy_reweighting==true){
				bf=min(1.0, bf*exp(current_wang_landau_system.wang_landau_potential[old_state_index]-current_wang_landau_system.wang_landau_potential[new_state_index])); //modify boltzmann factor according to wang-landau algorithm, according to grand canonical simulation paper "Density-of-states Monte Carlo method for simulation of fluids"
				//this makes the new state being accepted with the conditinal probability bf (bf is a transition probability = conditional probability from the old state to move to the new state)
			}else{
				bf=min(1.0, bf*exp(-beta*(E_pot_new-E_pot_old)));
			}
		}else{
			if(current_wang_landau_system.histogram[new_state_index]>=0 &&current_wang_landau_system.histogram[old_state_index]<0 )
				bf=10;//this makes the reaction get accepted, since we found a state in Gamma
			else if (current_wang_landau_system.histogram[new_state_index]<0 &&current_wang_landau_system.histogram[old_state_index]<0)
				bf=10;//accept, in order to be able to sample new configs, which might lie in Gamma
			else if(current_wang_landau_system.histogram[new_state_index]<0 &&current_wang_landau_system.histogram[old_state_index]>=0)
				bf=-10;//this makes the reaction get rejected, since the new state is not in Gamma while the old sate was in Gamma
		}
		
	}else if(old_state_index<0 && new_state_index>=0){
		bf=10;	//this makes the reaction get accepted, since we found a state in Gamma
	}else if(old_state_index<0 && new_state_index<0){
		bf=10;	//accept, in order to be able to sample new configs, which might lie in Gamma
	}else if(old_state_index>=0 && new_state_index<0){
		bf=-10; //this makes the reaction get rejected, since the new state is not in Gamma while the old sate was in Gamma
	}
	
	bool got_accepted=false;
	if(d_random()<bf){
		//accept
		if(new_state_index>=0 ){
			if(current_wang_landau_system.histogram[new_state_index]>=0){
				got_accepted=true;
				current_wang_landau_system.histogram[new_state_index]+=1;
				current_wang_landau_system.wang_landau_potential[new_state_index]+=current_wang_landau_system.wang_landau_parameter;
			}
		}
	}else{
		//reject
		if(old_state_index>=0){
			if(current_wang_landau_system.histogram[old_state_index]>=0){
				current_wang_landau_system.histogram[old_state_index]+=1;
				current_wang_landau_system.wang_landau_potential[old_state_index]+=current_wang_landau_system.wang_landau_parameter;
			}
		}
		//create particles again at the positions they were
		for(int i=0;i<particle_number_of_type;i++){
			double pos_x=particle_positions[3*i];
			double pos_y=particle_positions[3*i+1];
			double pos_z=particle_positions[3*i+2];
			double pos_vec[3]={pos_x,pos_y,pos_z};
			place_particle(p_id_s_changed_particles[i],pos_vec);
		}
		//restore polymer particle again at original position
		if((start_id_polymer!=current_wang_landau_system.int_fill_value && end_id_polymer !=current_wang_landau_system.int_fill_value) && current_wang_landau_system.fix_polymer==false){
//			place_particle(random_polymer_particle_id, old_pos_polymer_particle);
			for(int i=start_id_polymer;i<=end_id_polymer;i++){
				double ppos[3];
				ppos[0]=old_pos_polymer_particle[3*i];
				ppos[1]=old_pos_polymer_particle[3*i+1];
				ppos[2]=old_pos_polymer_particle[3*i+2];
				place_particle(i,ppos);
			}
		}
		
	}
	return got_accepted;
}


bool do_local_mc_move_for_type(int type, int start_id_polymer, int end_id_polymer){
	current_wang_landau_system.monte_carlo_trial_moves+=1;
	int p_id;
	Particle part;
	
	int old_state_index=get_flattened_index_wang_landau_of_current_state();
	double E_pot_old=calculate_current_potential_energy_of_system(0);
	
	
	int particle_number_of_type;
	number_of_particles_with_type(type, &(particle_number_of_type));
	if(particle_number_of_type==0){
		bool got_accepted=false;
		//reject
		if(old_state_index>=0){
			if(current_wang_landau_system.histogram[old_state_index]>=0){
				current_wang_landau_system.histogram[old_state_index]+=1;
				current_wang_landau_system.wang_landau_potential[old_state_index]+=current_wang_landau_system.wang_landau_parameter;
			}
		}
		return got_accepted;
	}
	double particle_positions[3];
	int changed_particle_counter=0;
	int p_id_s_changed_particles[particle_number_of_type];

	int max_tries=100*particle_number_of_type;//important for very dense systems
	int attempts=0;

	while(changed_particle_counter<1){
		//save old_position
		if(changed_particle_counter==0){
			find_particle_type(type, &p_id);
		}else{
			//determine a p_id you have not touched yet
			while(is_in_list(p_id,p_id_s_changed_particles,changed_particle_counter)){
				find_particle_type(type, &p_id); //check wether you already touched this p_id
			}
		}
		
		get_particle_data(p_id, &part);
		double ppos[3];
		memmove(ppos, part.r.p, 3*sizeof(double));
		particle_positions[0]=ppos[0];
		particle_positions[1]=ppos[1];
		particle_positions[2]=ppos[2];	
		bool particle_inserted_too_close_to_another_one=true;
		while(particle_inserted_too_close_to_another_one==true && attempts<max_tries){
			//change particle position
			double pos_x=box_l[0]*bit_random_generator(); //here we use the r250 random generator with higher period by using box_l[0]*bit_random_generator()
			double pos_y=box_l[1]*d_random();
			double pos_z=box_l[2]*d_random();
			double new_pos[3]={pos_x, pos_y, pos_z};
			place_particle(p_id,new_pos);
			double d_min=distto(new_pos,p_id);
			attempts+=1;
			if(d_min>current_reaction_system.exclusion_radius){
				particle_inserted_too_close_to_another_one=false;
			}
		}
		
		p_id_s_changed_particles[changed_particle_counter]=p_id;
		changed_particle_counter+=1;
	}
	
	if(attempts==max_tries){
		//reversing
		//create particles again at the positions they were
		for(int i=0;i<1;i++){
			double pos_x=particle_positions[3*i];
			double pos_y=particle_positions[3*i+1];
			double pos_z=particle_positions[3*i+2];
			double pos_vec[3]={pos_x,pos_y,pos_z};
			place_particle(p_id_s_changed_particles[i],pos_vec);
		}
	}
	
	//change polymer conformation if start and end id are provided
	double old_pos_polymer_particle[3];
	int random_polymer_particle_id;
	
	if((start_id_polymer!=current_wang_landau_system.int_fill_value && end_id_polymer !=current_wang_landau_system.int_fill_value) && current_wang_landau_system.fix_polymer==false){
		random_polymer_particle_id=start_id_polymer+i_random(end_id_polymer-start_id_polymer+1);
		Particle part;
		get_particle_data(random_polymer_particle_id, &part);
		memmove(old_pos_polymer_particle, part.r.p, 3*sizeof(double));
		//move particle to new position nearby
		double random_direction_vector[3];
		double length_of_displacement=0.05;
		vec_random(random_direction_vector, length_of_displacement);
		double pos_x=old_pos_polymer_particle[0]+random_direction_vector[0];
		double pos_y=old_pos_polymer_particle[1]+random_direction_vector[1];
		double pos_z=old_pos_polymer_particle[2]+random_direction_vector[2];
		double new_pos[3]={pos_x, pos_y, pos_z};
		place_particle(random_polymer_particle_id,new_pos);
	}
	
	
	int new_state_index=get_flattened_index_wang_landau_of_current_state();
	
	double E_pot_new=calculate_current_potential_energy_of_system(0);

	double bf=1.0;
	if(old_state_index>=0 && new_state_index>=0){
		if(current_wang_landau_system.histogram[new_state_index]>=0 &&current_wang_landau_system.histogram[old_state_index]>=0 ){
			if(current_wang_landau_system.do_energy_reweighting==true){
				bf=min(1.0, bf*exp(current_wang_landau_system.wang_landau_potential[old_state_index]-current_wang_landau_system.wang_landau_potential[new_state_index])); //modify boltzmann factor according to wang-landau algorithm, according to grand canonical simulation paper "Density-of-states Monte Carlo method for simulation of fluids"
				//this makes the new state being accepted with the conditinal probability bf (bf is a transition probability = conditional probability from the old state to move to the new state)
			}else{
				double beta =1.0/current_reaction_system.temperature_reaction_ensemble;
				bf=min(1.0, bf*exp(-beta*(E_pot_new-E_pot_old)));
			}
		}else{
			if(current_wang_landau_system.histogram[new_state_index]>=0 &&current_wang_landau_system.histogram[old_state_index]<0 )
				bf=10;//this makes the reaction get accepted, since we found a state in Gamma
			else if (current_wang_landau_system.histogram[new_state_index]<0 &&current_wang_landau_system.histogram[old_state_index]<0)
				bf=10;//accept, in order to be able to sample new configs, which might lie in Gamma
			else if(current_wang_landau_system.histogram[new_state_index]<0 &&current_wang_landau_system.histogram[old_state_index]>=0)
				bf=-10;//this makes the reaction get rejected, since the new state is not in Gamma while the old sate was in Gamma
		}
		
	}else if(old_state_index<0 && new_state_index>=0){
		bf=10;	//this makes the reaction get accepted, since we found a state in Gamma
	}else if(old_state_index<0 && new_state_index<0){
		bf=10;	//accept, in order to be able to sample new configs, which might lie in Gamma
	}else if(old_state_index>=0 && new_state_index<0){
		bf=-10; //this makes the reaction get rejected, since the new state is not in Gamma while the old sate was in Gamma
	}
	
	bool got_accepted=false;
	if(d_random()<bf){
		//accept
		if(new_state_index>=0 ){
			if(current_wang_landau_system.histogram[new_state_index]>=0){
				got_accepted=true;
				current_wang_landau_system.histogram[new_state_index]+=1;
				current_wang_landau_system.wang_landau_potential[new_state_index]+=current_wang_landau_system.wang_landau_parameter;
			}
		}
	}else{
		//reject
		if(old_state_index>=0){
			if(current_wang_landau_system.histogram[old_state_index]>=0){
				current_wang_landau_system.histogram[old_state_index]+=1;
				current_wang_landau_system.wang_landau_potential[old_state_index]+=current_wang_landau_system.wang_landau_parameter;
			}
		}
		//create particles again at the positions they were
		double pos_x=particle_positions[0];
		double pos_y=particle_positions[1];
		double pos_z=particle_positions[2];
		double pos_vec[3]={pos_x,pos_y,pos_z};
		place_particle(p_id_s_changed_particles[0],pos_vec);
		//restore polymer particle again at original position
		if(start_id_polymer!=current_wang_landau_system.int_fill_value && end_id_polymer !=current_wang_landau_system.int_fill_value){
			place_particle(random_polymer_particle_id, old_pos_polymer_particle);
		}
		
	}
	return got_accepted;
}

bool do_HMC_move(){
	current_wang_landau_system.monte_carlo_trial_moves+=1;
	int old_state_index=get_flattened_index_wang_landau_of_current_state();
	double E_pot_old=calculate_current_potential_energy_of_system(0);
	
	double particle_positions[3*max_seen_particle];

	//save old_position and set random velocities
	for(int p_id=0; p_id<max_seen_particle+1; p_id++){
		//save old positions
		Particle part;
		get_particle_data(p_id, &part);
		double ppos[3];
		memmove(ppos, part.r.p, 3*sizeof(double));
		free_particle(&part);//fixes memory leak. if omitted a lot of memory is leaked
		particle_positions[3*p_id]=ppos[0];
		particle_positions[3*p_id+1]=ppos[1];
		particle_positions[3*p_id+2]=ppos[2];
		
		//create random velocity vector according to Maxwell Boltzmann distribution for components
		double vel[3];
		//we usse mass=1 for all particles, think about adapting this
		vel[0]=pow(2*PI*current_reaction_system.temperature_reaction_ensemble,-3.0/2.0)*gaussian_random()*time_step;//scale for internal use in espresso
		vel[1]=pow(2*PI*current_reaction_system.temperature_reaction_ensemble,-3.0/2.0)*gaussian_random()*time_step;//scale for internal use in espresso
		vel[2]=pow(2*PI*current_reaction_system.temperature_reaction_ensemble,-3.0/2.0)*gaussian_random()*time_step;//scale for internal use in espresso
		set_particle_v(p_id,vel);

	}
	mpi_integrate(20,-1); //-1 for recalculating forces, this should be a velocity verlet NVE-MD move => do not turn on an thermostat	
	
	int new_state_index=get_flattened_index_wang_landau_of_current_state();
	double E_pot_new=calculate_current_potential_energy_of_system(0);
	
//	printf("E_pot_old %f E_pot_new %f\n",E_pot_old, E_pot_new); //use this to check wether your MD timestep is big enough. There needs to be a change in the energy bigger than Delta E. If there is no change increase the timestep.
	
	double beta =1.0/current_reaction_system.temperature_reaction_ensemble;
	double bf=1.0;
	if(old_state_index>=0 && new_state_index>=0){
		if(current_wang_landau_system.histogram[new_state_index]>=0 &&current_wang_landau_system.histogram[old_state_index]>=0 ){
			if(current_wang_landau_system.do_energy_reweighting==true){
				bf=min(1.0, bf*exp(current_wang_landau_system.wang_landau_potential[old_state_index]-current_wang_landau_system.wang_landau_potential[new_state_index])); //modify boltzmann factor according to wang-landau algorithm, according to grand canonical simulation paper "Density-of-states Monte Carlo method for simulation of fluids"
				//this makes the new state being accepted with the conditinal probability bf (bf is a transition probability = conditional probability from the old state to move to the new state)
			}else{
				bf=min(1.0, bf*exp(-beta*(E_pot_new-E_pot_old)));
			}
		}else{
			if(current_wang_landau_system.histogram[new_state_index]>=0 &&current_wang_landau_system.histogram[old_state_index]<0 )
				bf=10;//this makes the reaction get accepted, since we found a state in Gamma
			else if (current_wang_landau_system.histogram[new_state_index]<0 &&current_wang_landau_system.histogram[old_state_index]<0)
				bf=10;//accept, in order to be able to sample new configs, which might lie in Gamma
			else if(current_wang_landau_system.histogram[new_state_index]<0 &&current_wang_landau_system.histogram[old_state_index]>=0)
				bf=-10;//this makes the reaction get rejected, since the new state is not in Gamma while the old sate was in Gamma
		}
		
	}else if(old_state_index<0 && new_state_index>=0){
		bf=10;	//this makes the reaction get accepted, since we found a state in Gamma
	}else if(old_state_index<0 && new_state_index<0){
		bf=10;	//accept, in order to be able to sample new configs, which might lie in Gamma
	}else if(old_state_index>=0 && new_state_index<0){
		bf=-10; //this makes the reaction get rejected, since the new state is not in Gamma while the old sate was in Gamma
	}
		
	bool got_accepted=false;
	if(d_random()<bf){
		//accept
		if(new_state_index>=0 ){
			if(current_wang_landau_system.histogram[new_state_index]>=0){
				got_accepted=true;
				current_wang_landau_system.histogram[new_state_index]+=1;
				current_wang_landau_system.wang_landau_potential[new_state_index]+=current_wang_landau_system.wang_landau_parameter;
			}
		}
	}else{
		//reject
		if(old_state_index>=0){
			if(current_wang_landau_system.histogram[old_state_index]>=0){
				current_wang_landau_system.histogram[old_state_index]+=1;
				current_wang_landau_system.wang_landau_potential[old_state_index]+=current_wang_landau_system.wang_landau_parameter;
			}
		}
		//create particles again at the positions they were
		for(int i=0;i<max_seen_particle;i++){
			double pos_x=particle_positions[3*i];
			double pos_y=particle_positions[3*i+1];
			double pos_z=particle_positions[3*i+2];
			double pos_vec[3]={pos_x,pos_y,pos_z};
			place_particle(i,pos_vec);
		}
		
	}
	return got_accepted;
}



int accepted_moves=0;
int tries=0;
bool got_accepted;

int do_reaction_wang_landau(){
	tries+=current_wang_landau_system.wang_landau_steps;
	for(int step=0;step<current_wang_landau_system.wang_landau_steps;step++){
		int reaction_id=i_random(current_reaction_system.nr_single_reactions+3); //without +1 only reactions are sampled. The +1 makes that also dislocation moves of particles (at a fixed particle number) are performed
		if(reaction_id<current_reaction_system.nr_single_reactions){
			got_accepted=generic_oneway_reaction_wang_landau(reaction_id);
		 	//according to de pablo also needs to be performed for the runs without energy reweighting for sampling the configurational partition function
		}else if(reaction_id==current_reaction_system.nr_single_reactions){
			if(current_wang_landau_system.counter_ion_type>=0)
				got_accepted=do_global_mc_move_for_type(current_wang_landau_system.counter_ion_type, current_wang_landau_system.polymer_start_id, current_wang_landau_system.polymer_end_id); //if polymer_start_id and polymer_end_id are not set by user no moves for the ids from [polymer_start_id,polymer_end_id] are performed, except they are of the counter ion type
			else
				got_accepted=do_HMC_move();
		}else if(reaction_id==current_reaction_system.nr_single_reactions+1){		
			//or alternatively
			if(current_wang_landau_system.counter_ion_type>=0){
//				got_accepted=do_local_mc_move_for_type(current_wang_landau_system.counter_ion_type, current_wang_landau_system.polymer_start_id, current_wang_landau_system.polymer_end_id); //if polymer_start_id and polymer_end_id are not set by user no moves for the ids from [polymer_start_id,polymer_end_id] are performed, except they are of the counter ion type
				got_accepted=do_global_mc_move_for_type(current_wang_landau_system.counter_ion_type, current_wang_landau_system.polymer_start_id, current_wang_landau_system.polymer_end_id);
			}else{
				got_accepted=do_HMC_move();
			}
		}else if(reaction_id==current_reaction_system.nr_single_reactions+2){	
			//or alternatively			
			got_accepted=do_HMC_move();
//			got_accepted=do_global_mc_move_for_type(current_wang_landau_system.counter_ion_type, current_wang_landau_system.polymer_start_id, current_wang_landau_system.polymer_end_id);
		}	
			//or alternatively if you are not doing energy reweighting
//			if(current_wang_landau_system.do_energy_reweighting==false){
//				mpi_integrate(150,-1);
//			}
		
		if(got_accepted==true){
			accepted_moves+=1;
		}
		
		if(can_refine_wang_landau_one_over_t()){
			if(achieved_desired_number_of_refinements_one_over_t()==true){//check for convergence
				write_wang_landau_results_to_file(current_wang_landau_system.output_filename);
			}
			refine_wang_landau_parameter_one_over_t();
		}
	}
	//write out preliminary wang-landau potential results
	if(current_wang_landau_system.monte_carlo_trial_moves%(90000)<=current_wang_landau_system.wang_landau_steps){
		write_wang_landau_results_to_file(current_wang_landau_system.output_filename);
	}
	if(current_wang_landau_system.monte_carlo_trial_moves%(90000)<=current_wang_landau_system.wang_landau_steps){
		printf("tries %d acceptance rate %f\n",tries, double(accepted_moves)/tries);
		fflush(stdout);
	}

//	if(current_wang_landau_system.monte_carlo_trial_moves%100000==0)
//		remove_bins_that_have_not_been_sampled();
	
	return 0;	
};


void free_wang_landau(){
	free(current_wang_landau_system.histogram);
	free(current_wang_landau_system.wang_landau_potential);
	for(int CV_i=0;CV_i<current_wang_landau_system.nr_collective_variables;CV_i++){
		collective_variable* current_collective_variable=current_wang_landau_system.collective_variables[CV_i];
		if(current_collective_variable->corresponding_acid_types!=NULL) { //check wether we have a collective variable which is of the type of a degree of association
			free(current_collective_variable->corresponding_acid_types);
		}
		if(current_collective_variable->energy_boundaries_filename!=NULL){//check wether we have a collective variable which is of the type of an energy
			free(current_collective_variable->energy_boundaries_filename);
		}
		free(current_collective_variable);
	}
	free(current_wang_landau_system.collective_variables);
	free(current_wang_landau_system.output_filename);
	if(current_wang_landau_system.minimum_energies_at_flat_index!=NULL) //only present in energy preparation run
		free(current_wang_landau_system.minimum_energies_at_flat_index);
	if(current_wang_landau_system.maximum_energies_at_flat_index!=NULL)
		free(current_wang_landau_system.maximum_energies_at_flat_index);	


}

//boring helper functions
double average_int_list(int* int_number_list, int len_int_nr_list){
	double result=0.0;
	int counter_allowed_entries=0;
	for(int i=0;i<len_int_nr_list;i++){
		if(int_number_list[i]>=0){ //checks for validity of index i (think of energy collective variables, in a cubic memory layout there will be indices which are not allowed by the energy boundaries. These values will be initalized with a negative fill value)
			result+=(double) int_number_list[i];
			counter_allowed_entries+=1;
			
		}
			
	}
	return result/counter_allowed_entries;
}

int find_minimum_in_int_list(int* list, int len){
	double minimum =list[0];
	for (int i=0;i<len;i++){
		if(minimum<0)
			minimum=list[i];//think of negative histogram values that indicate not allowed energies in the case of an energy observable
		if(list[i]<minimum && list[i]>=0)
			minimum=list[i];
	}
	return minimum;
}

bool can_refine_wang_landau_one_over_t(){
	if(find_minimum_in_int_list(current_wang_landau_system.histogram,current_wang_landau_system.len_histogram)>0){
		//could also check for find_minimum_in_int_list(current_wang_landau_system.histogram,current_wang_landau_system.len_histogram)>0.1*average_int_list(current_wang_landau_system.histogram,current_wang_landau_system.len_histogram)
		return true;
	}else{
		return false;	
	}
}


void reset_histogram(){
	printf("Histogram is flat. Refining. Previous wang_landau_parameter was %f.\n",current_wang_landau_system.wang_landau_parameter);
	fflush(stdout);
	for(int i=0;i<current_wang_landau_system.len_histogram;i++){
		if(current_wang_landau_system.histogram[i]>=0){//checks for validity of index i (think of energy collective variables, in a cubic memory layout there will be indices which are not allowed by the energy boundaries. These values will be initalized with a negative fill value)
			current_wang_landau_system.histogram[i]=0;
		}	
	}
	
}

void refine_wang_landau_parameter_one_over_t(){
	double monte_carlo_time = current_wang_landau_system.monte_carlo_trial_moves/current_wang_landau_system.used_bins;
	if ( current_wang_landau_system.wang_landau_parameter/2.0 <1.0/monte_carlo_time ){
		current_wang_landau_system.wang_landau_parameter= 1.0/monte_carlo_time;
	} else {
		reset_histogram();
		current_wang_landau_system.wang_landau_parameter= current_wang_landau_system.wang_landau_parameter/2.0;
	}
	current_wang_landau_system.already_refined_n_times+=1;
	//for numerical stability here we also subtract the minimum positive value of the wang_landau_potential from the wang_landau potential, allowed since only the difference in the wang_landau potential is of interest.
	double minimum_wang_landau_potential=find_minimum_non_negative_value(current_wang_landau_system.wang_landau_potential,current_wang_landau_system.len_histogram);
	for(int i=0;i<current_wang_landau_system.len_histogram;i++){
		if(current_wang_landau_system.wang_landau_potential[i]>=0)//check for wether we are in the valid range of the collective variable
			current_wang_landau_system.wang_landau_potential[i]-=minimum_wang_landau_potential;	
	} 
}

bool achieved_desired_number_of_refinements_one_over_t () {
	if(current_wang_landau_system.wang_landau_parameter < current_wang_landau_system.final_wang_landau_parameter) {
		return true;
	} else {
		return false;
	}

}

void unravel_index(int* len_dims, int ndims, int flattened_index, int* unraveled_index_out){
	//idea taken from http://codinghighway.com/2014/02/22/c-multi-dimensional-arrays-part-2-flattened-to-unflattened-index/
	int mul[ndims];
	mul[ndims-1]=1;
	for (int j = ndims-2; j >= 0; j--)
		mul[j] = mul[j+1]*len_dims[j+1];
	for (int j = 0; j < ndims; j++)
		unraveled_index_out[j]=(flattened_index/mul[j])%len_dims[j];
}


void write_wang_landau_results_to_file(char* full_path_to_output_filename){

	FILE* pFile;
	pFile = fopen(full_path_to_output_filename,"w");
	if (pFile==NULL){
		printf("ERROR: Wang-Landau file could not be written\n");
		fflush(stdout);
	}else{
		int nr_subindices_of_collective_variable[current_wang_landau_system.nr_collective_variables];
		for(int collective_variable_i=0;collective_variable_i<current_wang_landau_system.nr_collective_variables;collective_variable_i++){
			nr_subindices_of_collective_variable[collective_variable_i]=int((current_wang_landau_system.collective_variables[collective_variable_i]->CV_maximum-current_wang_landau_system.collective_variables[collective_variable_i]->CV_minimum)/current_wang_landau_system.collective_variables[collective_variable_i]->delta_CV)+1; //+1 for collecive variables which are of type degree of association
		}

		for(int flattened_index=0;flattened_index<current_wang_landau_system.len_histogram;flattened_index++){
			//unravel index
			if(abs(current_wang_landau_system.wang_landau_potential[flattened_index]-current_wang_landau_system.double_fill_value)>1){ //only output data if they are not equal to current_reaction_system.double_fill_value. This if ensures that for the energy observable not allowed energies (energies in the interval [global_E_min, global_E_max]) in the multidimensional wang landau potential are printed out, since the range [E_min(nbar), E_max(nbar)] for each nbar may be a different one
				int unraveled_index[current_wang_landau_system.nr_collective_variables];
				unravel_index(nr_subindices_of_collective_variable,current_wang_landau_system.nr_collective_variables,flattened_index,unraveled_index);
				//use unraveled index
				for(int i=0;i<current_wang_landau_system.nr_collective_variables;i++){
					fprintf(pFile, "%f ",unraveled_index[i]*current_wang_landau_system.collective_variables[i]->delta_CV+current_wang_landau_system.collective_variables[i]->CV_minimum);
				}
				fprintf(pFile, "%f \n", current_wang_landau_system.wang_landau_potential[flattened_index]);
			}
		}
		fflush(pFile);
		fclose(pFile);
	}

}

int update_maximum_and_minimum_energies_at_current_state(){
	if(current_wang_landau_system.minimum_energies_at_flat_index==NULL || current_wang_landau_system.maximum_energies_at_flat_index==NULL){
		current_wang_landau_system.minimum_energies_at_flat_index=(double*) calloc(1,sizeof(double)*current_wang_landau_system.len_histogram);
		current_wang_landau_system.maximum_energies_at_flat_index=(double*) calloc(1,sizeof(double)*current_wang_landau_system.len_histogram);
		for (int i = 0; i < current_wang_landau_system.len_histogram; i++){
 	 		current_wang_landau_system.minimum_energies_at_flat_index[i] =current_wang_landau_system.double_fill_value;
 	 		current_wang_landau_system.maximum_energies_at_flat_index[i] =current_wang_landau_system.double_fill_value;
		}
	}
	
	double E_pot_current=calculate_current_potential_energy_of_system(0);
	int index=get_flattened_index_wang_landau_of_current_state();

	//update stored energy values
	if( (( E_pot_current<current_wang_landau_system.minimum_energies_at_flat_index[index])|| abs(current_wang_landau_system.minimum_energies_at_flat_index[index] -current_wang_landau_system.double_fill_value)<0.0001) && abs(E_pot_current)>0.0001 ) {
		current_wang_landau_system.minimum_energies_at_flat_index[index]=E_pot_current;
	}
	if( ((E_pot_current>current_wang_landau_system.maximum_energies_at_flat_index[index]) || abs(current_wang_landau_system.maximum_energies_at_flat_index[index] -current_wang_landau_system.double_fill_value)<0.0001) && abs(E_pot_current)>0.0001 ) {
		current_wang_landau_system.maximum_energies_at_flat_index[index]= E_pot_current;
	}
	

	return 0;
}

void write_out_preliminary_energy_run_results (char* full_path_to_output_filename) {
	FILE* pFile;
	pFile = fopen(full_path_to_output_filename,"w");
	if(pFile==NULL){
		printf("ERROR: Wang-Landau file could not be written\n");
		fflush(stdout);
	}else{
		fprintf(pFile, "#nbar E_min E_max\n");
		int nr_subindices_of_collective_variable[current_wang_landau_system.nr_collective_variables];
		for(int collective_variable_i=0;collective_variable_i<current_wang_landau_system.nr_collective_variables;collective_variable_i++){
			nr_subindices_of_collective_variable[collective_variable_i]=int((current_wang_landau_system.collective_variables[collective_variable_i]->CV_maximum-current_wang_landau_system.collective_variables[collective_variable_i]->CV_minimum)/current_wang_landau_system.collective_variables[collective_variable_i]->delta_CV)+1; //+1 for collecive variables which are of type degree of association
		}
	
		for(int flattened_index=0;flattened_index<current_wang_landau_system.len_histogram;flattened_index++){
			//unravel index
			int unraveled_index[current_wang_landau_system.nr_collective_variables];
			unravel_index(nr_subindices_of_collective_variable,current_wang_landau_system.nr_collective_variables,flattened_index,unraveled_index);
			//use unraveled index
			for(int i=0;i<current_wang_landau_system.nr_collective_variables;i++){
				fprintf(pFile, "%f ",unraveled_index[i]*current_wang_landau_system.collective_variables[i]->delta_CV+current_wang_landau_system.collective_variables[i]->CV_minimum);
			}
			fprintf(pFile, "%f %f \n", current_wang_landau_system.minimum_energies_at_flat_index[flattened_index], current_wang_landau_system.maximum_energies_at_flat_index[flattened_index]);
		}
		fflush(pFile);
		fclose(pFile);
	}
}


int get_flattened_index_wang_landau_without_energy_collective_variable(int flattened_index_with_energy_collective_variable, int collective_variable_index_energy_observable){
	int nr_subindices_of_collective_variable[current_wang_landau_system.nr_collective_variables];
		for(int collective_variable_i=0;collective_variable_i<current_wang_landau_system.nr_collective_variables;collective_variable_i++){
			nr_subindices_of_collective_variable[collective_variable_i]=int((current_wang_landau_system.collective_variables[collective_variable_i]->CV_maximum-current_wang_landau_system.collective_variables[collective_variable_i]->CV_minimum)/current_wang_landau_system.collective_variables[collective_variable_i]->delta_CV)+1; //+1 for collecive variables which are of type degree of association
		}
	//unravel index
	int unraveled_index[current_wang_landau_system.nr_collective_variables];
	unravel_index(nr_subindices_of_collective_variable,current_wang_landau_system.nr_collective_variables,flattened_index_with_energy_collective_variable,unraveled_index);
	//use unraveled index
	int nr_collective_variables=current_wang_landau_system.nr_collective_variables-1; //forget the last collective variable (the energy collective variable)
	double current_state[nr_collective_variables];
	for(int i=0;i<nr_collective_variables;i++){
		current_state[i]=unraveled_index[i]*current_wang_landau_system.collective_variables[i]->delta_CV+current_wang_landau_system.collective_variables[i]->CV_minimum;
	}
	
	//get collective_variables_minimum_values
	double collective_variables_minimum_values[nr_collective_variables];
	for(int CV_i=0;CV_i<nr_collective_variables;CV_i++){
		collective_variables_minimum_values[CV_i]=current_wang_landau_system.collective_variables[CV_i]->CV_minimum;	
	}
	//get collective_variables_maximum_values
	double collective_variables_maximum_values[nr_collective_variables];
	for(int CV_i=0;CV_i<nr_collective_variables;CV_i++){
		collective_variables_maximum_values[CV_i]=current_wang_landau_system.collective_variables[CV_i]->CV_maximum;	
	}
	//get delta_collective_variables_values
	double delta_collective_variables_values[nr_collective_variables];
	for(int CV_i=0;CV_i<nr_collective_variables;CV_i++){
		delta_collective_variables_values[CV_i]=current_wang_landau_system.collective_variables[CV_i]->delta_CV;	
	}
	int index=get_flattened_index_wang_landau(current_state, collective_variables_minimum_values, collective_variables_maximum_values, delta_collective_variables_values, nr_collective_variables);
	return index;
}

//use with caution otherwise you produce unpyhsical results, do only use when you know what you want to do. This can make wang landau converge on a reduced set Gamma. use this function e.g. in do_reaction_wang_landau() for the diprotonic acid
void remove_bins_that_have_not_been_sampled(){
	int removed_bins=0;
	double beta=1.0/current_reaction_system.temperature_reaction_ensemble;
	double largest_wang_landau_potential_at_given_particle_number=find_maximum(current_wang_landau_system.wang_landau_potential,current_wang_landau_system.len_histogram);
	for(int k=0;k<current_wang_landau_system.len_histogram;k++){
		if(current_wang_landau_system.wang_landau_potential[k]==0){
			removed_bins+=1;
			// criterion is derived from the canonical partition function and the ration of two summands for the same particle number
			current_wang_landau_system.histogram[k]=current_wang_landau_system.int_fill_value;
			current_wang_landau_system.wang_landau_potential[k]=current_wang_landau_system.double_fill_value;
		}
		
	}
	//update used bins
	current_wang_landau_system.used_bins-=removed_bins;
}

