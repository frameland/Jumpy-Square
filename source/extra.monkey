Strict
Import vsat

Function VPlaySound:Void(sound:Sound, channel:Int = 0)
	#If TARGET = "html5"
		Return
	#End
	PlaySound(sound, channel)
End

Function DrawGlowRect:Void(x:Float, y:Float, w:Float, h:Float)
	If TheGlowImage = Null
		TheGlowImage = LoadImage("glow.png")
	End
	Local imgWidth:Int = TheGlowImage.Width()
	Local imgHeight:Int = TheGlowImage.Height()
	Local sx:Float = w / imgWidth
	Local sy:Float = h / imgHeight
	DrawImage(TheGlowImage, x, y - imgHeight/2, 0, sx, 1) 'topLeft -> topRight
	DrawImage(TheGlowImage, x, y + h - imgHeight/2, 0, sx, 1) 'bottomLeft -> bottomRight
	DrawImage(TheGlowImage, x + imgHeight/2, y, -90, sy, 1) 'topLeft -> bottomLeft
	DrawImage(TheGlowImage, x + w + imgHeight/2, y, -90, sy, 1) 'topLeft -> bottomLeft
	SetBlend(AdditiveBlend)
	DrawRectOutline(x,y,w,h)
	SetBlend(AlphaBlend)
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








Global TheGlowImage:Image
