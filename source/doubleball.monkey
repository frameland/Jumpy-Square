Strict
Import game
Import extra


Class DoubleBall Extends VSprite
	
	Field speed:Vec2
	
	Method New()
		Self.SetImage(RealPath("double.png"))
		MidHandleImage(image)
		Self.color.Set(Color.Yellow)
		speed = New Vec2(Vsat.ScreenWidth2 * 0.8, Vsat.ScreenHeight * 0.3)
		Reset()
	End
	
	Method Reset:Void()
		position.x = Vsat.ScreenWidth2
		position.y = -Height()/2
		Self.Alpha = 0.0
		active = False
		scale.Set(1.0, 1.0)
	End
	
	Method Start:Void()
		Reset()
		active = True
		Alpha = 1.0
		hasCollided = False
	End
	
	Method IsActive:Bool()
		Return active & color.Alpha > 0.01
	End
	
	Method HasCollided:Bool()
		Return hasCollided
	End
	
	Method CollidesWith:Bool(rect:VRect)
		Local size:Float = image.Width() * 0.9
		Local x:Float = position.x - size/2
		Local y:Float = position.y - size/2
		Local result:Bool = RectsOverlap(rect.position.x, rect.position.y, rect.size.x, rect.size.y, x, y, size, size)
		If result
			hasCollided = True
		End
		Return result
	End
	
	Method Update:Void(dt:Float)
		If Not Self.IsActive() Return
		
		If hasCollided
			Return
		End
		
		position.y += dt * speed.y
		If position.y - image.HandleY() > Vsat.ScreenHeight
			active = False
			Self.Alpha = 0.0
			Return
		End
		Local sin:Float = Sinr(position.y/Vsat.ScreenHeight*PI*2)
		position.x = Vsat.ScreenWidth2 + (sin * speed.x)
		
		rotation += 180 * dt
	End
	
	Private
	Field active:Bool
	Field hasCollided:Bool
	
End
