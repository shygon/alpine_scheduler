#include "scorer.hh"

Score SumHappinessScorer::calcRoomScore(s32 timeslot, s32 room) {
  Score score = 0;
  ID abstractID = m_sched.getAbstractID(timeslot, room);
  for (int i=1; i < m_params.roomSize; ++i) {
    score += singleScore(abstractID, m_sched.getID(timeslot, room, i));
  }
  return score;
}

Score SumHappinessScorer::calcScore() {
  Score score = 0;
  for (s32 t = 0; t < m_params.nTimeslots; ++t) {
    for (s32 r = 0; r < m_params.nRooms; ++r) {
      score += calcRoomScore(t, r);
    }
  }
  return score;
}

void SumHappinessScorer::prepareSetChange(s32 timeslot, s32 room, s32 seat, ID id) {
  m_preChangePartialScore = 0;
  m_useChange2 = false;
  prepareSetChangeImpl(m_change1, timeslot, room);
}

void SumHappinessScorer::prepareSwapChange(s32 timeslot1, s32 room1, s32 seat1,
                               s32 timeslot2, s32 room2, s32 seat2)
{
  m_preChangePartialScore = 0;
  prepareSetChangeImpl(m_change1, timeslot1, room1);
  m_useChange2 = timeslot1 != timeslot2 || room1 != room2;
  if (m_useChange2) {
    prepareSetChangeImpl(m_change2, timeslot2, room2);
  }
}

void SumHappinessScorer::tryChange() {
  Score newScore = calcRoomScore(m_change1.timeslot, m_change1.room);
  if (m_useChange2) {
    newScore += calcRoomScore(m_change2.timeslot, m_change2.room);
  }
  m_changeScoreDelta = newScore - m_preChangePartialScore;
  m_score += m_changeScoreDelta;
}

void SumHappinessScorer::undoChange() {
  m_score -= m_changeScoreDelta;
}

Score SumHappinessScorer::singleScore(ID abstractID, ID personID) {
  if (invalidID(abstractID) || invalidID(personID))
    return 0;
  return getRanking(personID, abstractID, m_params);
}

Score SumHappinessScorer::calcSingleScore(s32 timeslot, s32 room, s32 seat) {
  return singleScore(m_sched.getAbstractID(timeslot, room), m_sched.getID(timeslot, room, seat));
}

void SumHappinessScorer::prepareSetChangeImpl(possibleChange& change, s32 timeslot, s32 room) {
  change = possibleChange{timeslot, room};
  m_preChangePartialScore += calcRoomScore(timeslot, room);
}

MinHappinessBonusScorer::MinHappinessBonusScorer(Schedule& sched, const Params& params) :
  m_sched(sched), m_params(params),
  m_pointBonus(100 * params.maxNormScore * params.nRooms * params.roomSize) {
    dbg() << "Point bonus: " << m_pointBonus << endl;
    calcScorePerPerson();
    calcMaxScorePerPerson();
    recalcScore();
}

Score MinHappinessBonusScorer::calcScore() {
  calcScorePerPerson();
  Score minScore;
  int nPeople;
  ID firstPersonID;
  findMinPersonScore(minScore, nPeople, firstPersonID);
  return minScore * m_pointBonus;
}

ID MinHappinessBonusScorer::calcMinPersonScoreID() {
  Score minScore;
  int nPeople;
  ID firstPersonID;
  findMinPersonScore(minScore, nPeople, firstPersonID);
  return firstPersonID;
}

void MinHappinessBonusScorer::addScorePerPersonForRoom(s32 timeslot, s32 room) {
  ID abstractID = m_sched.getAbstractID(timeslot, room);
  for (int i=1; i < m_params.roomSize; ++i) {
    ID personID = m_sched.getID(timeslot, room, i);
    m_scorePerPerson[personID] += singleScore(abstractID, personID);
  }
}

void MinHappinessBonusScorer::calcMaxScorePerPerson() {
  s32 maxScoreParticipations = m_params.minParticipations;
  dbg() << "Max score participations:" << maxScoreParticipations << endl;

  m_maxScorePerPerson.clear();
  vector<Score> personScores;
  for (ID personID = 0; personID < m_params.nPeople; ++personID) {
    personScores.clear();
    for (ID abstractID = 0; abstractID < m_params.nAbstracts; ++abstractID) {
      personScores.push_back(getRanking(personID, abstractID, m_params));
    }
    sort(begin(personScores), end(personScores), std::greater<Score>());
    Score sumScore = 0;
    for (s32 i=0; i < maxScoreParticipations; ++i) {
      sumScore += personScores[i];
    }
    m_maxScorePerPerson.push_back(sumScore);
    dbg() << "ID:" << personID << " (orig:" << m_params.personIdToOrig[personID]
          << ") max: " << sumScore << " current:" << m_scorePerPerson[personID]
          << " ratio:" << (m_scorePerPerson[personID] / sumScore)
          << " bonus:" << ((m_scorePerPerson[personID] / sumScore) * m_pointBonus)
          << endl;
  }
}

void MinHappinessBonusScorer::calcScorePerPerson() {
  m_scorePerPerson.assign(m_params.nPeople, 0);
  for (s32 t = 0; t < m_params.nTimeslots; ++t) {
    for (s32 r = 0; r < m_params.nRooms; ++r) {
      addScorePerPersonForRoom(t, r);
    }
  }
}

void MinHappinessBonusScorer::findMinPersonScore(Score& minScore, int& nPeople, ID& firstPersonID) {
  minScore = m_pointBonus * 1000;
  nPeople = 0;
  firstPersonID = INVALID_ID;
  for (ID i=0; i < m_scorePerPerson.size(); ++i) {
    Score normalizedScore = m_scorePerPerson[i] / m_maxScorePerPerson[i];
    if (normalizedScore < minScore) {
      minScore = normalizedScore;
      firstPersonID = i;
      nPeople = 1;
    } else if (normalizedScore == minScore) {
      ++nPeople;
    }
  }
}

Score MinHappinessBonusScorer::singleScore(ID abstractID, ID personID) {
  if (invalidID(abstractID) || invalidID(personID))
    return 0;
  return getRanking(personID, abstractID, m_params);
}

Score MinHappinessBonusScorer::calcSingleScore(s32 timeslot, s32 room, s32 seat) {
  return singleScore(m_sched.getAbstractID(timeslot, room), m_sched.getID(timeslot, room, seat));
}
