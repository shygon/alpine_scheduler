#include "schedule.hh"

#include <iomanip>
#include <algorithm>

using namespace std;


// Schedule class for managing a round table schedule
void Schedule::setAllIDs(vector<ID> IDs) {
  ASSERT(IDs.size() == m_nTimeslots * m_nRooms * m_roomSize);
  for (s32 t = 0; t < m_nTimeslots; ++t) {
    for (s32 r = 0; r < m_nRooms; ++r) {
      for (s32 i = 0; i < m_roomSize; ++i) {
        ID id = IDs[(t * m_nRooms * m_roomSize) +
                    (r * m_roomSize) + i];
        setIDUnsafe(t, r, i, id);
      }
    }
  }
}

void Schedule::reset() {
  m_ids.assign(m_nTimeslots * m_nRooms * m_roomSize, INVALID_ID);
  m_freeIDs.assign(m_nTimeslots * m_nPeople, true);
  m_abstractCount.assign(m_nAbstracts, 0);
  m_maxAbstractScore.assign(m_nAbstracts, 0);
  m_personCount.assign(m_nPeople, 0);
  m_personAbstract.assign(m_nAbstracts * m_nPeople, false);

  for (ID personID = 0; personID < m_nPeople; ++personID) {
    for (ID abstractID = 0; abstractID < m_nAbstracts; ++abstractID) {
      m_maxAbstractScore[abstractID] += getRanking(personID, abstractID, m_params);
    }
  }
}

void Schedule::initState() {
  // Assign abstracts: Sort by max potential score. Assign all abstracts,
  // then assign best abstracts one by one, penalizing the max score when
  // an abstract is picked.
  vector<Score> abstractScores = m_maxAbstractScore;
  vector<ID> indexes(m_nAbstracts);
  for (size_t i = 0; i < indexes.size(); ++i)
    indexes[i] = i;
  sort(indexes.begin(), indexes.end(),
       [&](int i, int j) { return abstractScores[i] > abstractScores[j]; } );
  for (size_t i = 0; i < indexes.size(); ++i) {
    if (i < 5 || i > indexes.size() - 5 - 1)
      dbg() << setw(3) << i << " i:" << setw(3) << indexes[i]
            << " score:" << abstractScores[indexes[i]] << endl;
  }

  size_t i = 0;
  for (s32 r = 0; r < m_nRooms; ++r) {
    for (s32 t = 0; t < m_nTimeslots; ++t) {
      ID abstractID;
      do {
        abstractID = indexes[i];
        i = (i + 1) % indexes.size();
      } while (!isFreeID(t, abstractID));
      setID(t, r, 0, abstractID);
    }
  }

  // Assign people to rooms
  for (s32 t = 0; t < m_nTimeslots; ++t) {
    for (s32 r = 0; r < m_nRooms; ++r) {
      for (s32 i = 1; i < m_roomSize; ++i) {
        const size_t max_tries = 5;
        for (size_t j=0; j<max_tries; ++j) {
          ID personID = getRandomFreePerson(t);
          if (setIDIfLegal(t, r, i, personID)) {
            break;
          } else if (j + 1 == max_tries) {
            dbg() << "Couldn't assign person to timeslot:" << t << " room:" << r << " seat:" << i << endl;
          }
        }
      }
    }
  }
}

bool Schedule::setIDIfLegal(s32 timeslot, s32 room, s32 seat, ID newID) {
  int i = idIndex(timeslot, room, seat);
  ID oldID = m_ids[i];
  bool newIDValid = validID(newID);
  bool oldIDValid = validID(oldID);
  if (newIDValid) {
    ASSERT(isFreeID(timeslot, newID));
    ASSERT(seat != 0 || newID < m_nAbstracts);
  }
  if (oldID == newID)
    return true;
  if (seat == 0) {
    if (newIDValid) {
      for (s32 s = 1; s < m_roomSize; ++s) {
        if (testPersonAbstractIfValid(getID(timeslot, room, s), newID)) {
          return false;
        }
      }
    }
    if (oldIDValid) {
      if (getAbstractCount(oldID) <= 1) {
        return false;
      }
    }
    if (newIDValid) {
      if (getAbstractCount(newID) >= m_params.maxPresentations) {
        return false;
      }
    }
  } else {
    ID abstractID = getAbstractID(timeslot, room);
    if (newIDValid) {
      if (testPersonAbstractIfValid(newID, abstractID)){
        return false;
      }
      if (getPersonCount(newID) >= m_params.maxParticipations) {
        return false;
      }
    }
    if (oldIDValid) {
      if (getPersonCount(oldID) <= m_params.minParticipations) {
        return false;
      }
    }
  }
  setIDUnsafe(timeslot, room, seat, newID);
  return true;
}

void Schedule::setIDUnsafe(s32 timeslot, s32 room, s32 seat, ID newID) {
  int i = idIndex(timeslot, room, seat);
  ID oldID = m_ids[i];
  bool newIDValid = validID(newID);
  bool oldIDValid = validID(oldID);
  if (seat == 0) {
    if (oldIDValid) {
      --m_abstractCount[oldID];
    }
    if (newIDValid) {
      ++m_abstractCount[newID];
    }
    for (s32 s = 1; s < m_roomSize; ++s) {
      setPersonAbstractIfValid(getID(timeslot, room, s), oldID, false);
      setPersonAbstractIfValid(getID(timeslot, room, s), newID, true);
    }
  } else {
    ID abstractID = getAbstractID(timeslot, room);
    if (newIDValid) {
      ++m_personCount[newID];
      setPersonAbstractIfValid(newID, abstractID, true);
    }
    if (oldIDValid) {
      --m_personCount[oldID];
      setPersonAbstractIfValid(oldID, abstractID, false);
    }
  }
  if (oldIDValid) setFreeID(timeslot, oldID, true);
  if (newIDValid) setFreeID(timeslot, newID, false);
  m_ids[i] = newID;
}

bool Schedule::validate() {
  for (size_t i=0; i<m_abstractCount.size(); ++i) {
    if (m_abstractCount[i] < 1) {
      err() << "Abstract ID" << i << " is never presented" << endl;
      return false;
    }
  }
  return true;
}

void Schedule::outputRoom(ostream& s, s32 timeslot, s32 room) const {
  outputRoomIDs(s, timeslot, room, m_ids);
}

void Schedule::outputIDs(ostream& s, const vector<ID>& ids) const {
  for (s32 t = 0; t < m_nTimeslots; ++t) {
    for (s32 r = 0; r < m_nRooms; ++r) {
      outputRoom(s, t, r);
    }
  }
}

void Schedule::outputRoomIDs(ostream& s, s32 timeslot, s32 room, const vector<ID>& ids) const {
  for (s32 i = 0; i < m_roomSize; ++i) {
    s << (i == 0 ? "" : ",")
      << m_params.personIdToOrig[ids.at(idIndex(timeslot, room, i))];
  }
  s << endl;
}
