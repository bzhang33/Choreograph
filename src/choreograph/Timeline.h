/*
 * Copyright (c) 2014 David Wicks, sansumbrella.com
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * conditions are met:
 *
 * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once
#include "Motion.h"
#include "Cue.h"

namespace choreograph
{

class Timeline;

///
/// Options for manipulating newly created TimelineItems.
/// Uses CRTP so we don't lose the actual type when chaining methods.
/// Do not store the TimelineOptions object, as it contains a non-owning reference.
///
template<typename SelfT>
class TimelineOptions
{
public:
  TimelineOptions( TimelineItem &item ):
    _item( item )
  {}

  //=================================================
  // TimelineItem Interface Mirroring.
  //=================================================

  /// Set whether the item should be removed from the timeline on finish.
  SelfT& removeOnFinish( bool doRemove ) { _item.setRemoveOnFinish( doRemove ); return self(); }

  /// Set the rate at which time advances for Motion.
  SelfT& playbackSpeed( Time speed ) { _item.setPlaybackSpeed( speed ); return self(); }

  /// Set the initial time offset of the TimelineItem.
  /// For Cues, this sets the time in the future.
  /// For Motions, this is akin to adding a hold at the beginning of the Sequence.
  SelfT& setStartTime( Time t ) { _item.setStartTime( t ); return self(); }

  /// Returns a shared_ptr to the control object for the Item. Allows you to cancel the Item later.
  TimelineItemControlRef  getControl() { return _item.getControl(); }

  /// Returns an object that cancels the Cue when it falls out of scope.
  /// You should store a ScopedCueRef in any class that captures [this] in a cued lambda.
  ScopedCancelRef         getScopedControl() { return std::make_shared<ScopedCancel>( _item.getControl() ); }

private:
  TimelineItem &_item;
  SelfT& self() { return static_cast<SelfT&>( *this ); }
};

///
/// MotionOptions provide a temporary facade for manipulating a timeline Motion and its underlying Sequence.
/// All methods return a reference back to the MotionOptions object for chaining.
/// Do not store the MotionOptions object, as it contains non-owning references.
///
template<typename T>
class MotionOptions : public TimelineOptions<MotionOptions<T>>
{
public:
  using SelfT = MotionOptions<T>;
  using MotionCallback = typename Motion<T>::Callback;

  MotionOptions( Motion<T> &motion, Sequence<T> &sequence, const Timeline &timeline ):
    TimelineOptions<MotionOptions<T>>( motion ),
    _motion( motion ),
    _sequence( sequence ),
    _timeline( timeline )
  {}

  //=================================================
  // Motion Interface Mirroring.
  //=================================================

  /// Set function to be called when Motion starts. Receives reference to motion.
  SelfT& startFn( const MotionCallback &fn ) { _motion.setStartFn( fn ); return *this; }

  /// Set function to be called when Motion updates. Receives current target value.
  SelfT& updateFn( const typename Motion<T>::DataCallback &fn ) { _motion.setUpdateFn( fn ); return *this; }

  /// Set function to be called when Motion finishes. Receives reference to motion.
  SelfT& finishFn( const MotionCallback &fn ) { _motion.setFinishFn( fn ); return *this; }

  /// Set a function to be called when the current inflection point is crossed.
  /// An inflection occcurs when the Sequence moves from one Phrase to the next.
  /// You must add a phrase after this for the inflection to occur.
  SelfT& onInflection( const MotionCallback &fn ) { return onInflection( _sequence.getPhraseCount(), fn ); }
  /// Adds an inflection callback when the specified phrase index is crossed.
  SelfT& onInflection( size_t point, const MotionCallback &fn ) { _motion.addInflectionCallback( point, fn ); return *this; }

  /// Clip the motion in \t time from the current Motion playhead.
  /// Also discards any phrases we have already played up to this point.
  SelfT& cutIn( Time t ) { _motion.cutIn( t ); return *this; }

  /// Clip the motion at time \t from the beginning of the Motion's Sequence.
  /// When used after Timeline::apply, will have the same effect as cutIn().
  SelfT& cutAt( Time t ) { _motion.sliceSequence( 0, t ); return *this; }

  //=================================================
  // Sequence Interface Mirroring.
  //=================================================

  /// Set the current value of the Sequence. Acts as an instantaneous hold.
  SelfT& set( const T &value ) { _sequence.set( value ); return *this; }

  /// Construct and append a Phrase to the Sequence.
  template<template <typename> class PhraseT, typename... Args>
  SelfT& then( const T &value, Time duration, Args&&... args ) { _sequence.template then<PhraseT>( value, duration, std::forward<Args>(args)... ); return *this; }

  /// Append a phrase to the Sequence.
  SelfT& then( const PhraseRef<T> &phrase ) { _sequence.then( phrase ); return *this; }

  /// Append a sequence to the Sequence.
  SelfT& then( const Sequence<T> &sequence ) { _sequence.then( sequence ); return *this; }

  //=================================================
  // Extra Sugar.
  //=================================================

  /// Append a Hold to the end of the Sequence. Assumes you want to hold using the Sequence's current end value.
  SelfT& hold( Time duration ) { _sequence.template then<Hold>( _sequence.getEndValue(), duration ); return *this; }

  /// Set the start time of this motion to the current end of all motions of \a other.
  template<typename U>
  SelfT& after( U *other );

  //=================================================
  // Accessors to Motion and Sequence.
  //=================================================

  Sequence<T>& getSequence() { return _sequence; }
  Motion<T>&   getMotion() { return _motion; }

private:
  Motion<T>       &_motion;
  Sequence<T>     &_sequence;
  const Timeline  &_timeline;
};

///
/// CueOptions provide a facade for manipulating a timeline Cue.
/// Non-get* methods return a reference back to the CueOptions object for chaining.
/// Do not store the CueOptions object, as it contains a non-owning reference.
///
class CueOptions : public TimelineOptions<CueOptions>
{
public:
  explicit CueOptions( Cue &cue ):
    TimelineOptions<CueOptions>( cue ),
    _cue( cue )
  {}

private:
  Cue  &_cue;
};

///
/// Timeline holds a collection of TimelineItems and updates them through time.
/// TimelineItems include Motions and Cues.
/// Motions can be cancelled by disconnecting their Output<T>.
/// Cues can be cancelled by using their control object.
/// Public methods are safe to call from cues and motion callbacks unless otherwise noted.
///
/// Timelines are move-only types because they contain unique_ptrs.
///
class Timeline
{
public:
  Timeline() = default;
  Timeline( const Timeline &rhs ) = delete;
  Timeline( Timeline &&rhs );
  //=================================================
  // Creating Motions. Output<T>* Versions
  //=================================================

  /// Apply a source to output, overwriting any previous connections.
  template<typename T>
  MotionOptions<T> apply( Output<T> *output );

  /// Apply a source to output, overwriting any previous connections.
  template<typename T>
  MotionOptions<T> apply( Output<T> *output, const Sequence<T> &sequence );

  template<typename T>
  MotionOptions<T> apply( Output<T> *output, const PhraseRef<T> &phrase );

  /// Add phrases to the end of the Sequence currently connected to \a output.
  template<typename T>
  MotionOptions<T> append( Output<T> *output );

  //=================================================
  // Creating Cues.
  //=================================================

  /// Add a cue to the timeline. It will be called after \a delay time elapses on this Timeline.
  CueOptions cue( const std::function<void ()> &fn, Time delay );

  //=================================================
  // Adding TimelineItems.
  //=================================================

  /// Add an item to the timeline. Called by append/apply/cue methods.
  /// Use to pass in MotionGroups and other types that Timeline doesn't create.
  void add( TimelineItemUniqueRef item );

  /// Add a timeline to the timeline.
  /// Wraps the timeline in a MotionGroup item.
  /// Note that this invalidates the passed-in timeline.
  void add( Timeline &&timeline );

  //=================================================
  // Time manipulation.
  //=================================================

  /// Advance all current items by \a dt time.
  /// Recommended method of updating the timeline.
  /// Do not call from a callback.
  void step( Time dt );

  /// Set all motions to \a time.
  /// Useful for scrubbing Timelines with non-removed items.
  /// Ignores the playback speed of TimelineItems, as it calls TimelineItem::jumpTo.
  /// Do not call from a callback.
  void jumpTo( Time time );

  //=================================================
  // Timeline querying methods and callbacks.
  //=================================================

  /// Returns true iff there are no items on this timeline.
  bool empty() const { return _items.empty(); }

  /// Returns the number of items on this timeline.
  size_t size() const { return _items.size(); }

  /// Sets a function to be called when this timeline becomes empty.
  /// It is safe to destroy the timeline from this callback, unlike any Cue.
  void setFinishFn( const std::function<void ()> &fn ) { _finish_fn = fn; }

  /// Returns the time (from now) at which all TimelineItems on this timeline will be finished.
  /// Cannot take into account Cues or Callbacks that may change the Timeline before finish.
  /// Useful information to cache when scrubbing Timelines with non-removed items.
  Time timeUntilFinish() const;

  Time getDuration() const;

  //=================================================
  // Timeline element manipulation.
  //=================================================

  /// Set whether motions should be removed when finished. Default is true.
  /// This value will be passed to all future TimelineItems created by the timeline.
  /// Does not affect TimelineItems already on the Timeline.
  void setDefaultRemoveOnFinish( bool doRemove = true ) { _default_remove_on_finish = doRemove; }

  /// Remove all items from this timeline.
  /// Do not call from a callback.
  void clear() { _items.clear(); }

  //=================================================
  // Creating Motions. T* Versions.
  // Prefer the Output<T>* versions over these.
  //=================================================

  /// Apply a source to output, overwriting any previous connections. Raw pointer edition.
  /// Unless you have a strong need, prefer the use of apply( Output<T> *output ) over this version.
  template<typename T>
  MotionOptions<T> applyRaw( T *output );

  /// Apply a source to output, overwriting any previous connections.
  template<typename T>
  MotionOptions<T> applyRaw( T *output, const Sequence<T> &sequence );

  /// Add phrases to the end of the Sequence currently connected to \a output. Raw pointer edition.
  /// Unless you have a strong need, prefer the use of append( Output<T> *output ) over this version.
  template<typename T>
  MotionOptions<T> appendRaw( T *output );

  std::vector<TimelineItemUniqueRef>::iterator begin() { return _items.begin(); }
  std::vector<TimelineItemUniqueRef>::iterator end( ) { return _items.end( ); }
  std::vector<TimelineItemUniqueRef>::const_iterator begin( ) const { return _items.cbegin( ); }
  std::vector<TimelineItemUniqueRef>::const_iterator end( ) const { return _items.cend( ); }

private:
  // True if Motions should be removed from timeline when they reach their endTime.
  bool                                _default_remove_on_finish = true;
  std::vector<TimelineItemUniqueRef>  _items;

  // queue to make adding cues from callbacks safe. Used if modifying functions are called during update loop.
  std::vector<TimelineItemUniqueRef>  _queue;
  bool                                _updating = false;
  std::function<void ()>              _finish_fn = nullptr;


  // Clean up finished motions and add queued motions after update.
  // Calls finish function if we went from having items to no items this iteration.
  void postUpdate();

  // Remove any motions that have stale pointers or that have completed playing.
  void removeFinishedAndInvalidMotions();

  // Move any items in the queue to our active items collection.
  void processQueue();

  /// Returns a non-owning raw pointer to the Motion applied to \a output, if any.
  /// If there is no Motion applied, returns nullptr.
  /// Used internally when appending to motions.
  template<typename T>
  Motion<T>* find( T *output ) const;

  /// Remove motion associated with specific output. Do not call from Callbacks.
  /// Used internally for raw pointer animation.
  void remove( void *output );
};

//=================================================
// Timeline Template Function Implementation.
//=================================================

template<typename T>
MotionOptions<T> Timeline::apply( Output<T> *output )
{
  auto motion = std::make_unique<Motion<T>>( output );

  auto &motion_ref = *motion;
  add( std::move( motion ) );

  return MotionOptions<T>( motion_ref, motion_ref.getSequence(), *this );
}

template<typename T>
MotionOptions<T> Timeline::apply( Output<T> *output, const PhraseRef<T> &phrase )
{
  auto motion = std::make_unique<Motion<T>>( output, Sequence<T>( phrase ) );

  auto &motion_ref = *motion;
  add( std::move( motion ) );

  return MotionOptions<T>( motion_ref, motion_ref.getSequence(), *this );
}

template<typename T>
MotionOptions<T> Timeline::apply( Output<T> *output, const Sequence<T> &sequence )
{
  auto motion = std::make_unique<Motion<T>>( output, sequence );

  auto &motion_ref = *motion;
  add( std::move( motion ) );

  return MotionOptions<T>( motion_ref, motion_ref.getSequence(), *this );
}

template<typename T>
MotionOptions<T> Timeline::append( Output<T> *output )
{
  auto motion = output->inputPtr();
  if( motion ) {
    return MotionOptions<T>( *motion, motion->getSequence(), *this );
  }
  return apply( output );
}

template<typename T>
MotionOptions<T> Timeline::applyRaw( T *output )
{ // Remove any existing motions that affect the same variable.
  // This is a raw pointer, so we don't know about any prior relationships.
  remove( output );

  auto motion = std::make_unique<Motion<T>>( output );

  auto &m = *motion;
  add( std::move( motion ) );

  return MotionOptions<T>( m, m.getSequence(), *this );
}

template<typename T>
MotionOptions<T> Timeline::applyRaw( T *output, const Sequence<T> &sequence )
{ // Remove any existing motions that affect the same variable.
  remove( output );
  auto motion = std::make_unique<Motion<T>>( output, sequence );

  auto &m = *motion;
  add( std::move( motion ) );

  return MotionOptions<T>( m, m.getSequence(), *this );
}

template<typename T>
MotionOptions<T> Timeline::appendRaw( T *output )
{
  auto motion = find( output );
  if( motion ) {
    return MotionOptions<T>( *motion, motion->getSequence(), *this );
  }
  return apply( output );
}

template<typename T>
Motion<T>* Timeline::find( T *output ) const
{
  for( auto &m : _items ) {
    if( m->getTarget() == output ) {
      return dynamic_cast<Motion<T>*>( m.get() );
    }
  }
  return nullptr;
}

//=================================================
// Additional MotionOptions Implementation.
//=================================================

template<typename T>
template<typename U>
MotionOptions<T>& MotionOptions<T>::after( U *other )
{
  auto ptr = _timeline.find( other );
  if( ptr ) {
    _motion->setStartTime( ptr->getTimeUntilFinish() );
  }
  return *this;
}

} // namespace choreograph
