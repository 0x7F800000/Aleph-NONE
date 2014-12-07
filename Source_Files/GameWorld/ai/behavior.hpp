#pragma once

class monsterBehavior
{
public:
 monsterBehavior() {}
 static class monsterBehavior *Get(const char* className, const char* behaviorName);
 
 virtual int behave(class Monster *me, ...) = 0;
 
 inline const char* getName()
 {
  return behaviorName;
 }
 
 inline const char* getClass()
 {
  return behaviorClass;
 }
 
 inline bool isExclusive()
 {
  return !behaviorPrecedence;
 }
 
 inline uint32 getPrecedence()
 {
  return behaviorPrecedence;
 }
 
protected:
 /*
  the name of the behavior
 */
 char* behaviorName;  
 
 /*
  the behavior's class
 */
 char* behaviorClass;
 /*
  behaviorPrecendence determines which behaviors of a class are exhibited before the others
  the lower the number, the more likely the behavior is to be exhibited before another behavior of its class
  
  ex: two behaviors of behaviorClass "Movement"
  {
   behaviorName: "Fly",
   behaviorClass: "Movement",
   behaviorPrecedence: 2
  }
  {
   behaviorName: "Float",
   behaviorClass: "Movement",
   behaviorPrecedence: 1
  }
  the "Float" behavior will be called before the "Fly" behavior because Float has a higher precedence
  
  A behaviorPrecedence of 0 means that the behavior's class is exclusive in that there should only ever be
  one behavior of that class attached to the Monster
  
  if a behavior with a precedence of 0 occupies the same class as another behavior that the Monster has,
  an assertion should fail.
 */
 uint32 behaviorPrecedence;
};
