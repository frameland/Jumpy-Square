Strict
Import vsat

Function VPlaySound:Void(sound:Sound, channel:Int = 0)
	#If TARGET = "html5"
		Return
	#End
	PlaySound(sound, channel)
End


Class MoveDownTransition Extends VTransition
	
	Field startPoint:Float = 0
	
	Method New()
		Super.New()
	End
	
	Method New(duration:Float)
		Super.New(duration)
	End
	
	Method Update:Void(dt:Float)
		Super.Update(dt)
		scene.OnUpdate(dt)
	End
	
	Method Render:Void()
		PushMatrix()
		ResetMatrix()
		ResetBlend()
		Local progress:Float = Tweening(easingType, Time, 0.0, 1.0, Duration)
		Translate(0, Vsat.ScreenHeight * progress + startPoint)
		scene.OnRender()
		PopMatrix()
	End
	
	Method SetScene:Void(scene:VScene)
		Self.scene = scene
	End
	
	Method EasingType:Void(type:Int) Property
		easingType = type
	End
	
	Private
	Field scene:VScene
	Field easingType:Int = EASE_OUT_EXPO
End


Class MoveUpTransition Extends MoveDownTransition
	
	Method New()
		Super.New()
	End
	
	Method New(duration:Float)
		Super.New(duration)
	End
	
	Method Render:Void()
		PushMatrix()
		ResetMatrix()
		ResetBlend()
		Local progress:Float = Tweening(easingType, Time, 0.0, 1.0, Duration)
		Translate(0, -Vsat.ScreenHeight * progress + startPoint)
		scene.OnRender()
		PopMatrix()
	End

End















