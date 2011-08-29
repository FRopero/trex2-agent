/*********************************************************************
 * Software License Agreement (BSD License)
 * 
 *  Copyright (c) 2011, MBARI.
 *  All rights reserved.
 * 
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 * 
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of the TREX Project nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 * 
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */
#include "EuropaReactor.hh"
#include "DbSolver.hh"

#include "bits/europa_convert.hh"

#include <PLASMA/PlanDatabase.hh>
#include <PLASMA/Token.hh>
#include <PLASMA/TokenVariable.hh>

#include <PLASMA/Debug.hh>

using namespace TREX::europa;
using namespace TREX::transaction;
using namespace TREX::utils;

/*
 * class TREX::europa::EuropaReactor
 */ 

// structors 

EuropaReactor::EuropaReactor(xml_arg_type arg)
  :TeleoReactor(arg, false), m_assembly(*this), m_core(*this), 
   m_filter(NULL) {
  bool found;
  std::string nddl;
  rapidxml::xml_node<> const &cfg = TeleoReactor::xml_factory::node(arg);
  rapidxml::xml_attribute<> const *model = cfg.first_attribute("model");

  if( NULL!=model ) {
    std::string file_name(model->value(), model->value_size());
    
    if( file_name.empty() )
      throw XmlError(cfg, "Attribute model is empty");
    nddl = manager().use(file_name, found);
    if( !found ) 
      throw XmlError(cfg, "Unable to locate model file \""+file_name+"\"");
  } else {
    std::string short_nddl = getName().str()+".nddl",
      long_nddl = getGraphName().str()+"."+short_nddl;
    // locate nddl model 
    //   - 1st look for <agent>.<reactor>.nddl
    nddl = manager().use(long_nddl, found);
    if( !found ) {
      // if not found look for jsut <reactor>.nddl
      nddl = manager().use(short_nddl, found);
      if( !found )
	// no model found for this reactor 
	throw ReactorException(*this, "Unable to locate \""+long_nddl+"\" or \""+
			       short_nddl+"\"");
    }
  }
  // Load the model
  if( !m_assembly.playTransaction(nddl) )
    throw ReactorException(*this, "model in "+nddl+" is inconsistent.");
  
  if( !m_assembly.plan_db()->isClosed() ) {
    syslog("WARN")<<"Plan Database is not closed:\n"
		  <<"\tClosing it now!!\n";
    m_assembly.plan_db()->close();
  }

  // Now load solver configuration 
  std::string solver_cfg = parse_attr<std::string>(xml_factory::node(arg), 
						   "solverConfig");  
  if( solver_cfg.empty() )
    throw XmlError(xml_factory::node(arg), "solverConfig attribute is empty");
  DeliberationFilter::set_current(this);
  m_assembly.configure_solver(solver_cfg);
  // finally I identify the external end internal timelines
  std::list<EUROPA::ObjectId> objs;
  m_assembly.trex_timelines(objs);
  syslog()<<"Found "<<objs.size()<<" TREX timeline declarations";
  
  for(std::list<EUROPA::ObjectId>::const_iterator it=objs.begin(); 
      objs.end()!=it; ++it) {
    EUROPA::LabelStr name = (*it)->getName();
    Symbol trex_name(name.c_str()), mode_val;
    EUROPA::ConstrainedVariableId mode = m_assembly.mode(*it);    
    if( !mode->isSpecified() ) 
      throw ReactorException(*this, 
			     "Mode of timeline "+trex_name.str()+" is not specified");
    else {
      EUROPA::DataTypeId type = mode->getDataType();
      mode_val = type->toString(mode->getSpecifiedValue());
    }
       
    if( Assembly::EXTERNAL_MODE==mode_val || Assembly::OBSERVE_MODE==mode_val ) {
      use(trex_name, Assembly::OBSERVE_MODE!=mode_val);      
      m_core.set_external(*it); 
    } else if( Assembly::INTERNAL_MODE==mode_val ) {
      m_assembly.check_default(*it);
      provide(trex_name);
      if( !isInternal(trex_name) ) {
	// Formally it would be better to demote it as External
	// but for now we will just hide this timeline to the rest of the world
	syslog("WARN")<<"Unable to declare "<<trex_name<<" as Internal ...\n"
		      <<"\t making it Private.";
      } else 
        m_core.set_internal(*it); 
    } else if( Assembly::IGNORE_MODE==mode_val ) {
      m_assembly.ignore(*it);
    } else {
      // everything else is just private ...
      if( Assembly::PRIVATE_MODE!=mode_val ) 
	// should never happen
	syslog("WARN")<<"timeline "<<trex_name<<" mode \""<<mode_val<<"\" is unknown.\n"
		      <<"\tI'll assume it is Private.";
    }
  }
}

EuropaReactor::~EuropaReactor() {}

bool EuropaReactor::in_scope(EUROPA::TokenId const &tok) {
  TREX::transaction::TICK 
    deadline = getCurrentTick()+getExecLatency()+getLookAhead();
  if( !m_assembly.ignored(tok) ) {
    EUROPA::IntervalIntDomain start_t = tok->start()->lastDomain(), 
      end_t = tok->end()->lastDomain();
    return end_t.getUpperBound() >= getCurrentTick() &&
      ( start_t.getUpperBound() < getFinalTick() ||
	start_t.getLowerBound() <=deadline );
  }
  return false;
}

bool EuropaReactor::dispatch_window(EUROPA::ObjectId const &obj, 
				    TREX::transaction::TICK &from, 
				    TREX::transaction::TICK &to) {
  TREX::utils::Symbol name(obj->getName().toString());
  external_iterator pos = find_external(name);
  if( pos.valid() && pos->accept_goals() ) {
    TREX::transaction::IntegerDomain window = pos->dispatch_window(getCurrentTick());
    TREX::transaction::IntegerDomain::bound 
      lo = window.lowerBound(), hi = window.upperBound();
    from = lo.value();
    if( hi.isInfinity() )
      to = getFinalTick();
    else
      to = hi.value();
    return true;
  }
  return false;
}

// callbacks

//  - TREX transaction callback 

void EuropaReactor::notify(Observation const &o) {
  m_assembly.setStream();
  {
    std::pair<EUROPA::ObjectId, EUROPA::TokenId> 
      ret = m_assembly.convert(o, true);

    if( ret.second.isId() ) {
      ret.second->start()->restrictBaseDomain(EUROPA::IntervalIntDomain(getCurrentTick(),
							       getCurrentTick()));
      ret.second->end()->restrictBaseDomain(EUROPA::IntervalIntDomain(getCurrentTick()+1,
								      PLUS_INFINITY));
      m_core.notify(ret);
    } else {
      syslog("ERROR")<<"Failed to produce observation "
		     <<o.object()<<'.'<<o.predicate()<<" inside europa model.";
    }
  }
}

void EuropaReactor::handleRequest(goal_id const &g) {
  m_assembly.setStream();
  {
    EUROPA::TokenId tok = m_assembly.convert(*g, false).second;
    
    if( tok.isId() ) {
      // restrict start, duration and end
      try {
	details::europa_restrict(tok->start(), g->getStart());
	details::europa_restrict(tok->duration(), g->getDuration());
	details::europa_restrict(tok->end(), g->getEnd());
      } catch(DomainExcept const &de) {
	syslog("ERROR")<<"Failed to restrict goal "<<g->object()<<'.'<<g->predicate()
		       <<'['<<g<<"] temporal attributes: "<<de;
	return;
      }
      m_internal_goals[tok->getKey()] = g;
      if( m_assembly.inactive() )
	m_assembly.mark_active();
    } else {
      syslog("WARN")<<"Ignored unknown goal "
		    <<g->object()<<'.'<<g->predicate();
    }
  }
}

void EuropaReactor::handleRecall(goal_id const &g) {
  m_assembly.setStream();
  
  for(europa_mapping::iterator i=m_internal_goals.begin(); 
      m_internal_goals.end()!=i; ++i)
    if( i->second==g ) {
      EUROPA::eint key = i->first;
      m_internal_goals.erase(i);
      
      // Need to notify the europa solver
      m_core.recall(key);
      return;
    }
}

//  - TREX execution callbacks

void EuropaReactor::handleInit() {
  m_assembly.setStream();
  {
    
    // Now is the time to set my timing constants in the model
    EUROPA::PlanDatabaseId db = m_assembly.plan_db();
    
    
    EUROPA::ConstrainedVariableId 
      mission_end = db->getGlobalVariable(Assembly::MISSION_END),
      tick_duration = db->getGlobalVariable(Assembly::TICK_DURATION), 
      clock_var = db->getGlobalVariable(Assembly::CLOCK_VAR);
    
    if( mission_end.isNoId() )
      throw ReactorException(*this, "Unable to locate "+
			     Assembly::MISSION_END.toString()+
			     " in the model");
    if( tick_duration.isNoId() )
      throw ReactorException(*this, "Unable to locate "+
			     Assembly::TICK_DURATION.toString()+
			     " in the model");
    if( clock_var.isNoId() )
      syslog("WARN")<<"Unable to locate "<<Assembly::CLOCK_VAR.toString()
		    <<"in the model";
    mission_end->restrictBaseDomain(EUROPA::IntervalIntDomain(getFinalTick(), 
							      getFinalTick()));
    tick_duration->restrictBaseDomain(EUROPA::IntervalIntDomain(tickDuration(),
								tickDuration()));
    // prepare the clock variable
    clock_var->restrictBaseDomain(EUROPA::IntervalIntDomain(getInitialTick(),
							    getFinalTick()));
    // Next thing is to process facts
    m_core.initialize(clock_var);
  }
}

void EuropaReactor::handleTickStart() {
  m_assembly.setStream();
  
  if( getCurrentTick()==getInitialTick() )
    reset_deliberation();
  m_completedThisTick = false;
  TREX::transaction::TICK 
    deadline = getCurrentTick()+getExecLatency()+getLookAhead();
  
  m_filter->set_horizon(getCurrentTick()+1, std::min(getFinalTick(), deadline));
  m_core.doDispatch();
}

bool EuropaReactor::synchronize() {
  m_assembly.setStream();
  // m_filter->set_horizon(getCurrentTick(), getCurrentTick()+1);
  if( !m_core.synchronize()  ) {
    m_assembly.solver().reset();
    // doRecalls
    m_assembly.mark_inactive();
    if( !( m_core.relax(false) && m_core.synchronize()) ) {
      m_assembly.mark_inactive();
      if( !(m_core.relax(true) && m_core.synchronize()) ) {
        syslog()<<"Unable to explain the current state of the world.";
        return false;
      }
    }
  }
  // TREX::transaction::TICK 
  //   deadline = getCurrentTick()+getExecLatency()+getLookAhead();
  
  // m_filter->set_horizon(getCurrentTick(), std::min(getFinalTick(), deadline));
  m_core.doNotify();
  // m_core.archive();
  logPlan();
  return true;
}

bool EuropaReactor::hasWork() {
  m_assembly.setStream();
  if( m_assembly.invalid() )
    return false;
  if( m_assembly.active() )
    return true;
  return !m_completedThisTick;
}

void EuropaReactor::resume() {
  m_assembly.setStream();
  // syslog()<<"step "<<m_steps;
  if( m_assembly.invalid() ) {
    syslog("ERROR")<<"Cannot resume deliberation with an invalid database.";
    return;
  }
  if( m_assembly.solver().isExhausted() ) {
    syslog("WARN")<<"No plan found.";
    m_assembly.mark_invalid();
  }
  ++m_steps;
  if( !m_core.step() ) {
    if( !m_core.relax(false) ) 
      if( !m_core.relax(true) ) {
	syslog("ERROR")<<"Unable to recover from a plan inconsistancy.";
	return;
      }
    m_assembly.mark_active();
  }
}

//  - Europa interface callbacks

void EuropaReactor::removed(EUROPA::TokenId const &tok) {
  EUROPA::TokenId active = tok;
  if( tok->isMerged() )
    active = tok->getActiveToken();
  TICK end_time = EUROPA::cast_basis(active->end()->lastDomain().getUpperBound());
  int delay = getCurrentTick()-end_time;

  europa_mapping::iterator i = m_external_goals.find(active->getKey());
  if( m_external_goals.end()!=i ) {
    m_external_goals.erase(i);
  } else {
    i = m_internal_goals.find(tok->getKey());
    if( m_internal_goals.end()!=i ) {
      if( delay>0 ) 
	syslog("goals")<<"Request ["<<i->second<<"] completed ("<<delay
		       <<" ticks ago).";
      else
	syslog("goals")<<"Request ["<<i->second<<"] completed.";
      m_internal_goals.erase(i);
    }
  }
}

void EuropaReactor::request(EUROPA::ObjectId const &tl, 
			    EUROPA::TokenId const &tok) {
  EUROPA::TokenId active = tok;
  if( tok->isMerged() )
    active = tok->getActiveToken();

  europa_mapping::iterator i = m_external_goals.find(active->getKey());
  if( m_external_goals.end()==i ) {
    TREX::utils::Symbol name(tl->getName().toString());
    Goal myGoal(name, tok->getUnqualifiedPredicateName().toString());
    
    goal_id request = postGoal(myGoal);
    if( request )
      m_external_goals[active->getKey()] = request;
  } 
}

void EuropaReactor::relax() {
  for(europa_mapping::const_iterator i=m_external_goals.begin();
      m_external_goals.end()!=i; ++i) {
    syslog("recall")<<"RECALL "<<i->second<<"(Europa ID : "<<i->first<<").";
    postRecall(i->second);    
  }
  m_external_goals.clear();  
}

void EuropaReactor::recall(EUROPA::TokenId const &tok) {
  EUROPA::TokenId active = tok;
  if( tok->isMerged() )
    active = tok->getActiveToken();

  europa_mapping::iterator i = m_external_goals.find(active->getKey());
  if( m_external_goals.end()!=i ) {
    goal_id g = i->second;
    m_external_goals.erase(i);
    syslog("recall")<<"RECALL "<<i->second;
    postRecall(g);
  }
}

void EuropaReactor::notify(EUROPA::ObjectId const &tl, 
			   EUROPA::TokenId const &tok) {
  Observation obs(tl->getName().c_str(), 
		  tok->getUnqualifiedPredicateName().toString());
  
  // populate the observation 

  postObservation(obs);
}


bool EuropaReactor::deactivate_solver() {
  m_assembly.solver().clear();
  return true;
}
