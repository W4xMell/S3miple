// -*-c++-*-

/*
 *Copyright:

 Copyright (C) Hiroki SHIMORA

 This code is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 3, or (at your option)
 any later version.

 This code is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this code; see the file COPYING.  If not, write to
 the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.

 *EndCopyright:
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <cstring>
#include "sample_field_evaluator.h"

#include "field_analyzer.h"
#include "simple_pass_checker.h"

#include <rcsc/player/player_evaluator.h>
#include <rcsc/common/server_param.h>
#include <rcsc/common/logger.h>
#include <rcsc/math_util.h>

#include <iostream>
#include <algorithm>
#include <cmath>
#include <cfloat>

// #define DEBUG_PRINT

using namespace rcsc;

static const int VALID_PLAYER_THRESHOLD = 8;


/*-------------------------------------------------------------------*/
/*!

 */
static double evaluate_state( const PredictState & state,  const std::vector<ActionStatePair> &path );


/*-------------------------------------------------------------------*/
/*!

 */
SampleFieldEvaluator::SampleFieldEvaluator()
{

}

/*-------------------------------------------------------------------*/
/*!

 */
SampleFieldEvaluator::~SampleFieldEvaluator()
{

}

/*-------------------------------------------------------------------*/
/*!

 */
double
SampleFieldEvaluator::operator()( const PredictState & state,
                                  const std::vector< ActionStatePair > & path ) const
{
    const double final_state_evaluation = evaluate_state( state, path );

    //
    // ???
    //

    double result = final_state_evaluation;

    return result;
}

static double
getLengthPenalty(const PredictState &first_state, const std::vector<ActionStatePair> &path)
{
    double path_length_penalty = 0;
    const PredictState last_state = path.back().state();
    const double spent_time_penalty = -0.15*last_state.spendTime();
    if (path.size() > 2)
    {
        path_length_penalty -= 3.0;
    }
    if (path.size() >3)
    {
        path_length_penalty -= 4.0;
    }
    if (path.size() >4)
    {
        path_length_penalty -= 5.0;
    }
    if (path.size() >5)
    {
        path_length_penalty -= 6.0;
    }

    return path_length_penalty+spent_time_penalty;
}
/*-------------------------------------------------------------------*/
/*!

 */
static
double
evaluate_state( const PredictState & state, const std::vector<ActionStatePair> &path )
{
    const ServerParam & SP = ServerParam::i();

    const AbstractPlayerObject * holder = state.ballHolder();

#ifdef DEBUG_PRINT
    dlog.addText( Logger::ACTION_CHAIN,
                  "========= (evaluate_state) ==========" );
#endif

    //
    // if holder is invalid, return bad evaluation
    //
    if ( ! holder )
    {
#ifdef DEBUG_PRINT
        dlog.addText( Logger::ACTION_CHAIN,
                      "(eval) XXX null holder" );
#endif
        return - DBL_MAX / 2.0;
    }

    const int holder_unum = holder->unum();


    //
    // ball is in opponent goal
    //
    if ( state.ball().pos().x > + ( SP.pitchHalfLength() - 0.1 )
         && state.ball().pos().absY() < SP.goalHalfWidth() + 2.0 )
    {
#ifdef DEBUG_PRINT
        dlog.addText( Logger::ACTION_CHAIN,
                      "(eval) *** in opponent goal" );
#endif
        return +1.0e+7;
    }

    //
    // ball is in our goal
    //
    if ( state.ball().pos().x < - ( SP.pitchHalfLength() - 0.1 )
         && state.ball().pos().absY() < SP.goalHalfWidth() )
    {
#ifdef DEBUG_PRINT
        dlog.addText( Logger::ACTION_CHAIN,
                      "(eval) XXX in our goal" );
#endif

        return -1.0e+7;
    }


    //
    // out of pitch
    //
    if ( state.ball().pos().absX() > SP.pitchHalfLength()
         || state.ball().pos().absY() > SP.pitchHalfWidth() )
    {
#ifdef DEBUG_PRINT
        dlog.addText( Logger::ACTION_CHAIN,
                      "(eval) XXX out of pitch" );
#endif

        return - DBL_MAX / 2.0;
    }


    //
    // set basic evaluation
    //
    double point = state.ball().pos().x;

    point += std::max( 0.0,
                       40.0 - ServerParam::i().theirTeamGoalPos().dist( state.ball().pos() ) );

#ifdef DEBUG_PRINT
    dlog.addText( Logger::ACTION_CHAIN,
                  "(eval) ball pos (%f, %f)",
                  state.ball().pos().x, state.ball().pos().y );

    dlog.addText( Logger::ACTION_CHAIN,
                  "(eval) initial value (%f)", point );
#endif

    //
    // add bonus for goal, free situation near offside line
    //
    if ( FieldAnalyzer::can_shoot_from
         ( holder->unum() == state.self().unum(),
           holder->pos(),
           state.getPlayerCont( new OpponentOrUnknownPlayerPredicate( state.ourSide() ) ),
           VALID_PLAYER_THRESHOLD ) )
    {
        point += 1.0e+6;
#ifdef DEBUG_PRINT
        dlog.addText( Logger::ACTION_CHAIN,
                      "(eval) bonus for goal %f (%f)", 1.0e+6, point );
#endif

        if ( holder_unum == state.self().unum() )
        {
            point += 5.0e+5;
#ifdef DEBUG_PRINT
            dlog.addText( Logger::ACTION_CHAIN,
                          "(eval) bonus for goal self %f (%f)", 5.0e+5, point );
#endif
        }
    }

    Line2D holdder_ball(state.ball().pos(), holder->pos());
    const double y = holdder_ball.getY(ServerParam::i().pitchHalfLength());

    if (state.getOpponentGoalie() != NULL)
    {
        if (y > SP.pitchHalfWidth()|| y < -SP.pitchHalfWidth())
        {
            if (holder->pos().dist(Vector2D(ServerParam::i().pitchHalfLength(), y)) < 8
                && holder->pos().x > 48
                && state.getOpponentGoalie()->pos().x > holder->pos().x
                && state.getOpponentGoalie()->pos().absY() > 6.02
                && state.getOpponentGoalie()->pos().y * holder->pos().y > 0
                && state.getOpponentGoalie()->pos().dist(Vector2D(ServerParam::i().pitchHalfLength(), y))< 1.5)
            {
                return -DBL_MAX/2;
            }
            
        }
        
    }
    
    double point2 = 1000.0;
    double ball_posX = state.ball().pos().x,
                ball_posabsY = state.ball().pos().absY();
    int weight = 1;
    int role = state.self().unum();
    double offsideline = state.offsideLineX();

    if (path.empty())
    {
        return -1.0e+7;
    }

    if(path.size() > 0)
    {
        const CooperativeAction &a = path[0].action();
        switch (a.category())
        {
        case CooperativeAction::Hold:
            weight = 1.05;
            break;
        case CooperativeAction::Dribble:
            if (ball_posX > 35 && ball_posX < 45 && (role == 9 || role == 10)
                && ball_posabsY < 18
                && strcmp(a.description(), "SelfPass") == 0)
            {
                weight = 2.0;
            }
            break;
        case CooperativeAction::Pass:
            if(ball_posX > 10 && ball_posX < 35 && (role == 9 || role == 10)
                && role == 7 && (a.targetPlayerUnum() == 9 || a.targetPlayerUnum() == 10))
            {
                weight == 2.5;
            }
            break;
        default:
            break;
        }
    }
    
    point += point2*weight;
    point += getLengthPenalty(state, path);
    return point;
}
