Strict
Import framework

'--------------------------------------------------------------------------
' * Interface
'--------------------------------------------------------------------------
Interface VActionEventHandler
	Method OnActionEvent:Void(id:Int, action:VAction)
End




'--------------------------------------------------------------------------
' * Base Class for Actions
'--------------------------------------------------------------------------
Class VAction Abstract
	
	Const STARTED:Int = 1
	Const FINISHED:Int = 2
	Const LOOPED:Int = 3
	
	Method Update:Void(dt:Float)
	End
	
	Method IsActive:Bool() Property
		Return active
	End
	
	Method Duration:Float() Property
		Return duration
	End
	
	Method ElapsedTime:Float() Property
		Return time
	End
	
	Method CustomObject:Object() Property
		Return customObject
	End
	
	Method CustomObject:Void(object:Object) Property
		customObject = object
	End
	
	Method Name:String() Property
		Return name
	End
	
	Method Name:Void(name:String) Property
		Self.name = name
	End
	
	Method Start:Void()
		active = True
		If listener
			listener.OnActionEvent(VAction.STARTED, Self)
		End
	End
	
	Method Stop:Void()
		active = False
		If listener
			listener.OnActionEvent(VAction.FINISHED, Self)
			If link Then link.Remove()
		End
	End
	
	Method Pause:Void()
		active = False
	End
	
	Method Resume:Void()
		active = True
	End
	
	Method Restart:Void()
		Resume()
		ResetTime()
	End
	
	Method ResetTime:Void()
		time = 0.0
	End
	
	Method IncrementTime:Void(by:Float)
		time += by
		If (time >= duration) 
			time = duration
		End
	End
	
	Method SetListener:Void(listener:VActionEventHandler)
		Self.listener = listener
	End
	
	Method AddToList:Void(collection:List<VAction>)
		link = collection.AddLast(Self)
	End
	
	Function UpdateList:Void(collection:List<VAction>, dt:Float)
		For Local action:= EachIn collection
			action.Update(dt)
		Next
	End
	
	
	Private
	Field time:Float
	Field duration:Float
	Field active:Bool
	Field listener:VActionEventHandler
	Field link:ListNode<VAction> 'can be used for performance, see AddToList
	Field customObject:Object 'store extra data
	Field name:String
End




'--------------------------------------------------------------------------
' * Containers that can hold multiple actions
' * A sequence will finish an action, before starting to execute the next one
' * Groups execute their actions in parallel
' * Repeaters continue their action forever (until stopped)
'--------------------------------------------------------------------------
Class VActionSequence Extends VAction Implements VActionEventHandler
	
	Private
	Field sequence:List<VAction> = New List<VAction>
	
	Public
	Field forwardEvents:Bool = False
	
	Method New()
		Start()
	End
	
	Method New(withArray:VAction[])
		For Local i:Int = 0 Until withArray.Length
			AddAction(withArray[i])
		Next
		Start()
	End
	
	Method AddAction:Void(action:VAction)
		sequence.AddLast(action)
		action.SetListener(Self)
		duration += action.duration
	End
	
	Method Update:Void(dt:Float)
		If active
			time += dt
			Repeat
				If sequence.IsEmpty() Or sequence.Last().IsActive() = False
					Stop()
					Return
				End
				For Local a:= EachIn sequence
					If a.IsActive
						a.Update(dt)
						Return
					End
				Next
			Forever
		End
	End
	
	Method OnActionEvent:Void(id:Int, action:VAction)
		If forwardEvents = False
			Return
		End
		
		Select id
			Case VAction.FINISHED
				If listener
					listener.OnActionEvent(id, action)
				End
			Case VAction.STARTED
				If listener
					listener.OnActionEvent(id, action)
				End
		End
	End
	
	Method Restart:Void()
		Super.Restart()
		Local action:VAction
		For action = EachIn sequence
			action.Restart()
		Next
	End
	
End

Class VActionGroup Extends VAction Implements VActionEventHandler
	Private
	Field group:List<VAction> = New List<VAction>
	
	Public
	Field forwardEvents:Bool = False
	
	Method New()
		Start()
	End
	
	Method New(withArray:VAction[])
		For Local i:Int = 0 Until withArray.Length
			AddAction(withArray[i])
		Next
		Start()
	End
	
	Method AddAction:Void(action:VAction)
		group.AddLast(action)
		action.SetListener(Self)
		For Local a:= EachIn group
			duration = Max(duration, action.Duration)
		Next
	End
	
	Method Update:Void(dt:Float)
		If active
			time += dt
			Local updatedOneAtleast:Bool = False
			Local a:VAction
			For a = EachIn group
				If a.IsActive
					a.Update(dt)
					updatedOneAtleast = True
				End
			Next
			
			If updatedOneAtleast = False
				Stop()
			End
		End
	End
	
	Method OnActionEvent:Void(id:Int, action:VAction)
		If forwardEvents = False
			Return
		End
		
		Select id
			Case VAction.FINISHED
				If listener
					listener.OnActionEvent(id, action)
				End
			Case VAction.STARTED
				If listener
					listener.OnActionEvent(id, action)
				End
		End
	End
	
	Method Restart:Void()
		Super.Restart()
		Local action:VAction
		For action = EachIn group
			action.Restart()
		Next
	End
	
End

Class VActionRepeater Extends VAction Implements VActionEventHandler
	
	Private
	Field repeatAction:VAction
	
	Public
	Method New(repeatAction:VAction)
		Self.repeatAction = repeatAction
		repeatAction.SetListener(Self)
		Start()
	End
	
	Method Update:Void(dt:Float)
		If active
			time += dt
			repeatAction.Update(dt)
		End
	End
	
	Method Restart:Void()
		Super.Restart()
		repeatAction.Restart()
	End
	
	Method OnActionEvent:Void(id:Int, action:VAction)
		If id = VAction.FINISHED
			repeatAction = action
			Restart()
			If listener
				listener.OnActionEvent(VAction.LOOPED, action)
			End
		End
	End
	
End




'--------------------------------------------------------------------------
' * TweenAction with Vec2: Move, Scale, etc.
'--------------------------------------------------------------------------
Class VVec2Action Extends VAction
	
	Private
	Field pointer:Vec2
	Field moveBy:Vec2
	Field startPosition:Vec2
	Field lastPosition:Vec2
	Field easingType:Int
	
	Public
	Method New(vector:Vec2, moveX:Float, moveY:Float, duration:Float, easingType:Int, start:Bool = True)
		pointer = vector
		moveBy = New Vec2(moveX, moveY)
		Self.duration = duration
		Self.easingType = easingType
		If start
			Start()
		End
	End
	
	Method Update:Void(dt:Float)
		If active
			If Not startPosition
				startPosition = New Vec2(pointer)
				lastPosition = New Vec2(pointer)
			End
			
			IncrementTime(dt)
			
			Local x:Float = Tweening(easingType, time, startPosition.x, moveBy.x, duration)
			Local y:Float = Tweening(easingType, time, startPosition.y, moveBy.y, duration)
			pointer.Add(x - lastPosition.x, y - lastPosition.y)
			lastPosition.Set(x, y)
			
			If (time >= duration) 
				Stop()
			End
		End
	End
	
	Method Restart:Void()
		Super.Restart()
		startPosition = Null
	End
	
End

Class VVec2ToAction Extends VAction
	
	Private
	Field pointer:Vec2
	Field moveTo:Vec2
	Field moveChange:Vec2
	Field startPosition:Vec2
	Field lastPosition:Vec2
	Field easingType:Int
	
	Public
	Method New(vector:Vec2, moveToX:Float, moveToY:Float, duration:Float, easingType:Int, start:Bool = True)
		pointer = vector
		moveTo = New Vec2(moveToX, moveToY)
		Self.duration = duration
		Self.easingType = easingType
		If start
			Start()
		End
	End
	
	Method Update:Void(dt:Float)
		If active
			If Not startPosition
				startPosition = New Vec2(pointer)
				lastPosition = New Vec2(pointer)
				If moveChange = Null
					moveChange = New Vec2
				End
				moveChange.Set(moveTo.x - pointer.x, moveTo.y - pointer.y)
			End
			
			IncrementTime(dt)
			
			Local x:Float = Tweening(easingType, time, startPosition.x, moveChange.x, duration)
			Local y:Float = Tweening(easingType, time, startPosition.y, moveChange.y, duration)
			pointer.Add(x - lastPosition.x, y - lastPosition.y)
			lastPosition.Set(x, y)
			
			If (time >= duration) 
				Stop()
			End
		End
	End
	
	Method Restart:Void()
		Super.Restart()
		startPosition = Null
	End
	
End



'--------------------------------------------------------------------------
' * Rotation
'--------------------------------------------------------------------------
Class VRotateAction Extends VAction
	
	Private
	Field pointer:VEntity
	Field rotateBy:Float
	Field startRotation:Float = $ffffff 'Magic Number: used for first time use, see Update()
	Field lastRotation:Float
	Field easingType:Int
	
	Public
	Method New(rotatable:VEntity, rotateBy:Float, duration:Float, easingType:Int, start:Bool = True)
		pointer = rotatable
		Self.rotateBy = rotateBy
		Self.duration = duration
		Self.easingType = easingType
		If start
			Start()
		End
	End
	
	Method Update:Void(dt:Float)
		If active
			If startRotation = $ffffff
				startRotation = pointer.rotation
				lastRotation = startRotation
			End
			
			IncrementTime(dt)
			
			Local angle:Float = Tweening(easingType, time, startRotation, rotateBy, duration)
			pointer.rotation = angle - lastRotation + pointer.rotation
			lastRotation = angle

			If time >= duration
				Stop()
			End
		End
	End
	
	Method Restart:Void()
		Super.Restart()
		startRotation = $ffffff
	End
	
	
End

Class VRotateToAction Extends VAction
	
	Private
	Field pointer:VEntity
	Field rotateTo:Float
	Field rotateChange:Float
	Field startRotation:Float = $ffffff 'Magic Number: used for first time use, see Update()
	Field lastRotation:Float
	Field easingType:Int
	
	Public
	Method New(rotatable:VEntity, rotateTo:Float, duration:Float, easingType:Int, start:Bool = True)
		pointer = rotatable
		Self.rotateTo = rotateTo
		Self.duration = duration
		Self.easingType = easingType
		If start
			Start()
		End
	End
	
	Method Update:Void(dt:Float)
		If active
			If startRotation = $ffffff
				startRotation = pointer.rotation
				lastRotation = startRotation
				rotateChange = rotateTo - startRotation
			End
			
			IncrementTime(dt)
			
			Local angle:Float = Tweening(easingType, time, startRotation, rotateChange, duration)
			pointer.rotation = angle - lastRotation + pointer.rotation
			lastRotation = angle
			
			If (time >= duration) 
				Stop()
			End
		End
	End
	
	Method Restart:Void()
		Super.Restart()
		startRotation = $ffffff
	End
	
End



'--------------------------------------------------------------------------
' * Color
'--------------------------------------------------------------------------
Class VFadeAlphaAction Extends VAction

	Private
	Field pointer:Color
	Field fadeBy:Float
	Field startAlpha:Float = -1.0
	Field lastAlpha:Float
	Field easingType:Int
	
	Public
	Method New(color:Color, fadeBy:Float, duration:Float, easingType:Int, start:Bool = True)
		pointer = color
		Self.fadeBy = fadeBy
		Self.duration = duration
		Self.easingType = easingType
		If start
			Start()
		End
	End
	
	Method Update:Void(dt:Float)
		If active
			If startAlpha = -1.0
				startAlpha = pointer.Alpha
				lastAlpha = startAlpha
			End
			
			IncrementTime(dt)
			
			Local alpha:Float = Tweening(easingType, time, startAlpha, fadeBy, duration)
			pointer.Alpha += alpha - lastAlpha
			lastAlpha = alpha
			
			If (time >= duration) 
				Stop()
			End
		End
	End
	
	Method Restart:Void()
		Super.Restart()
		startAlpha = -1.0
	End
	
End

Class VFadeToAlphaAction Extends VAction

	Private
	Field pointer:Color
	Field fadeTo:Float
	Field fadeChange:Float
	Field startAlpha:Float = -1.0
	Field lastAlpha:Float
	Field easingType:Int
	
	Public
	Method New(color:Color, fadeTo:Float, duration:Float, easingType:Int, start:Bool = True)
		pointer = color
		Self.fadeTo = fadeTo
		Self.duration = duration
		Self.easingType = easingType
		If start
			Start()
		End
	End
	
	Method Update:Void(dt:Float)
		If active
			If startAlpha = -1.0
				startAlpha = pointer.Alpha
				lastAlpha = startAlpha
				fadeChange = fadeTo - startAlpha
			End
			
			IncrementTime(dt)
			
			Local alpha:Float = Tweening(easingType, time, startAlpha, fadeChange, duration)
			pointer.Alpha += alpha - lastAlpha
			lastAlpha = alpha
			
			If (time >= duration) 
				Stop()
			End
		End
	End
	
	Method Restart:Void()
		Super.Restart()
		startAlpha = -1.0
	End
	
End


Class VFadeColorAction Extends VAction

	Private
	Field pointer:Color
	Field fadeBy:Color
	Field startRed:Float = -1.0
	Field startGreen:Float = -1.0
	Field startBlue:Float = -1.0
	Field lastRed:Float
	Field lastGreen:Float
	Field lastBlue:Float
	Field easingType:Int
	
	Public
	Method New(color:Color, fadeBy:Color, duration:Float, easingType:Int, start:Bool = True)
		pointer = color
		Self.fadeBy = fadeBy
		Self.duration = duration
		Self.easingType = easingType
		If start
			Start()
		End
	End
	
	Method Update:Void(dt:Float)
		If active
			If startRed = -1.0
				startRed = pointer.Red
				startGreen = pointer.Green
				startBlue = pointer.Blue
				lastRed = startRed
				lastGreen = startGreen
				lastBlue = startBlue
			End
			
			IncrementTime(dt)
			
			Local red:Float = Tweening(easingType, time, startRed, fadeBy.Red, duration)
			pointer.Red += red - lastRed
			lastRed = red
			Local green:Float = Tweening(easingType, time, startGreen, fadeBy.Green, duration)
			pointer.Green += green - lastGreen
			lastGreen = green
			Local blue:Float = Tweening(easingType, time, startBlue, fadeBy.Blue, duration)
			pointer.Blue += blue - lastBlue
			lastBlue = blue
			
			If (time >= duration) 
				Stop()
			End
		End
	End
	
	Method Restart:Void()
		Super.Restart()
		startRed = -1.0
		startGreen = -1.0
		startBlue = -1.0
	End
	
End

Class VFadeToColorAction Extends VAction

	Private
	Field pointer:Color
	Field fadeTo:Color
	Field startRed:Float = -1.0
	Field startGreen:Float = -1.0
	Field startBlue:Float = -1.0
	Field lastRed:Float
	Field lastGreen:Float
	Field lastBlue:Float
	Field fadeRed:Float
	Field fadeGreen:Float
	Field fadeBlue:Float
	Field easingType:Int
	
	Public
	Method New(color:Color, fadeTo:Color, duration:Float, easingType:Int, start:Bool = True)
		pointer = color
		Self.fadeTo = fadeTo
		Self.duration = duration
		Self.easingType = easingType
		If start
			Start()
		End
	End
	
	Method Update:Void(dt:Float)
		If active
			If startRed = -1.0
				startRed = pointer.Red
				startGreen = pointer.Green
				startBlue = pointer.Blue
				lastRed = startRed
				lastGreen = startGreen
				lastBlue = startBlue
				fadeRed = fadeTo.Red - startRed
				fadeGreen = fadeTo.Green - startGreen
				fadeBlue = fadeTo.Blue - startBlue
			End
			
			IncrementTime(dt)
			
			Local red:Float = Tweening(easingType, time, startRed, fadeRed, duration)
			pointer.Red += red - lastRed
			lastRed = red
			Local green:Float = Tweening(easingType, time, startGreen, fadeGreen, duration)
			pointer.Green += green - lastGreen
			lastGreen = green
			Local blue:Float = Tweening(easingType, time, startBlue, fadeBlue, duration)
			pointer.Blue += blue - lastBlue
			lastBlue = blue
			
			If (time >= duration) 
				Stop()
			End
		End
	End
	
	Method Restart:Void()
		Super.Restart()
		startRed = -1.0
		startGreen = -1.0
		startBlue = -1.0
	End
	
End


'--------------------------------------------------------------------------
' * Delay
'--------------------------------------------------------------------------
Class VDelayAction Extends VAction
	
	Method New(duration:Float, start:Bool = True)
		Self.duration = duration
		If start
			Start()
		End
	End
	
	Method Update:Void(dt:Float)
		If active
			time += dt
			If time >= duration
				Stop()
				Return
			End
		End
	End
	
End






