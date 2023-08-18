#pragma once

#include "defs.hh"
#include "params.hh"
#include "utils.hh"
#include <vector>


// Schedule class for managing a round table schedule
class Schedule final {
public:
  Schedule(const Params& params) :
    m_params(params), m_nPeople(params.nPeople), m_nAbstracts(params.nAbstracts),
    m_nTimeslots(params.nTimeslots), m_nRooms(params.nRooms), m_roomSize(params.roomSize),
    m_timeslotSeats(m_nRooms * m_roomSize) { reset(); }
  void setAllIDs(std::vector<ID> IDs);

  void reset();
  void initState();

  ID getRandomFreePerson(s32 timeslot) {
    ID i = randInt(m_nPeople);
    for (s32 p = i; p < m_nPeople; ++p) {
      if (isFreeID(timeslot, p))
        return p;
    }
    for (s32 p = 0; p < i; ++p) {
      if (isFreeID(timeslot, p))
        return p;
    }
    ASSERT(false); // No free person ID
    return 0;
  }

  bool testPersonAbstract(ID personID, ID abstractID) {
    return m_personAbstract.at(abstractID * m_nPeople + personID);
  }
  void setPersonAbstract(ID personID, ID abstractID, bool value=true) {
    m_personAbstract.at(abstractID * m_nPeople + personID) = value;
  }
  bool testPersonAbstractIfValid(ID personID, ID abstractID) {
    if (validID(personID) && validID(abstractID))
      return testPersonAbstract(personID, abstractID);
    return false;
  }
  void setPersonAbstractIfValid(ID personID, ID abstractID, bool value=true) {
    if (validID(personID) && validID(abstractID))
      setPersonAbstract(personID, abstractID, value);
  }
  void setID(s32 timeslot, s32 room, s32 seat, ID newID) {
    bool res;
    res = setIDIfLegal(timeslot, room, seat, newID);
    ASSERT(res);
    (void)res;
  }
  bool setIDIfLegal(s32 timeslot, s32 room, s32 seat, ID newID);
  void setIDUnsafe(s32 timeslot, s32 room, s32 seat, ID newID);
  ID getID(s32 timeslot, s32 room, s32 seat) const {
    return m_ids.at(idIndex(timeslot, room, seat));
  }
  ID getAbstractID(s32 timeslot, s32 room) const { return getID(timeslot, room, 0); }
  bool isFreeID(s32 timeslot, ID id) {
    return m_freeIDs.at(timeslot * m_nPeople + id);
  }
  s32 getAbstractCount(ID abstractID) { return m_abstractCount[abstractID]; }
  s32 getPersonCount(ID personID) { return m_personCount[personID]; }

  bool validate();

  void getAllIDs(std::vector<ID>& ids) const { ids = m_ids; }
  void output(std::ostream& s) const { outputIDs(s, m_ids); }
  void outputRoom(std::ostream& s, s32 timeslot, s32 room) const;

  void outputIDs(std::ostream& s, const std::vector<ID>& ids) const;

  void outputRoomIDs(std::ostream& s, s32 timeslot, s32 room, const std::vector<ID>& ids) const;

protected:

  void setFreeID(s32 timeslot, ID id, bool val) {
    m_freeIDs[timeslot * m_nPeople + id] = val;
  }
  int idIndex(s32 timeslot, s32 room, s32 seat) const {
    return timeslot * m_timeslotSeats +
           room * m_roomSize +
           seat;
  }

  const Params& m_params;
  const s32 m_nPeople, m_nAbstracts;
  const s32 m_nTimeslots, m_nRooms, m_roomSize, m_timeslotSeats;
  std::vector<ID> m_ids;
  std::vector<bool> m_freeIDs;
  std::vector<Score> m_maxAbstractScore;
  std::vector<s32> m_abstractCount;
  std::vector<s32> m_personCount;

  std::vector<s8> m_personAbstract;
};
