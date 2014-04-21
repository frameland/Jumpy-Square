Strict
Import vsat
Import extra

Class Player Extends VRect
	
	Field velocity:Vec2
	Field isJumping:Bool
	Field jumpForce:Vec2 = New Vec2(Vsat.ScreenWidth * 2, -Vsat.ScreenHeight)
	Field gravity:Float = Vsat.ScreenHeight / 27
	Field willJump:Bool
	Field lastPositions:List<Vec2>
	Field maxPositions:Int = 10
	Field jumpSound:Sound
	
	Field widthRelative:Float = 12.8
	Field heightRelative:Float = 9.6
	
	'helpers
	Field isIntroAnimating:Bool
	Field didTouchAnyWall:Bool = True
	
	
'--------------------------------------------------------------------------
' * Init & Helpers
'--------------------------------------------------------------------------
	Method New()
		Super.New(0, 0, Int(Vsat.ScreenWidth/widthRelative), Int(Vsat.ScreenHeight/heightRelative))
		color.Set(Color.NewBlue)
		renderOutline = True
		velocity = New Vec2
		lastPositions = New List<Vec2>
		jumpSound = LoadSound("jump.mp3")
	End
	
	Method Reset:Void()
		position.y = Vsat.ScreenHeight * 0.1
		velocity.Set(0,0)
		If Rnd() > 0.5
			SetLeft()
		Else
			SetRight()
		End
		lastPositions.Clear()
		willJump = False
	End
	

'--------------------------------------------------------------------------
' * Update
'--------------------------------------------------------------------------	
	Method UpdateLastPosition:Void()
		If Not isJumping
			maxPositions = 5
		Else
			maxPositions = 10
		End
		
		lastPositions.AddFirst(New Vec2(position))
		If lastPositions.Count() > maxPositions
			lastPositions.RemoveLast()
		End
	End
	
	Method UpdatePhysics:Void(dt:Float)
		UpdateLastPosition()
		
		If isJumping
			velocity.y += gravity * 1.4
			velocity.Limit(gravity * 50)
		Else
			velocity.y += gravity * 2
			velocity.Limit(gravity * 100)
		End
		
		position.Add(velocity.x * dt, velocity.y * dt)
		
		If (position.y - size.y * 0.3 > Vsat.ScreenHeight) Or (position.y < -Self.size.y)
			Local gameOver:= New VEvent
			gameOver.id = "GameOver"
			Vsat.FireEvent(gameOver)
		End
		
		'Stick to wall
		Local friction:Float = 0.2
		Local touchesAnyWall:Bool = False
		
		If Self.TouchesLeftWall()
			touchesAnyWall = True
			position.x = 1
		ElseIf Self.TouchesRightWall()
			touchesAnyWall = True
			position.x = Vsat.ScreenWidth - Self.size.x - 1
		End
		
		If touchesAnyWall
			If didTouchAnyWall = False
				OnWallImpact()
				didTouchAnyWall = True
			End
			
			'First reset position then apply velocity with friction
			position.Sub(velocity.x * dt, velocity.y * dt)
			position.Add(velocity.x * dt, velocity.y * dt * friction)
			isJumping = False
			If willJump Then Self.Jump()
		Else
			didTouchAnyWall = False
		End
	End

	Method Jump:Void()
		If Not isJumping
			VPlaySound(jumpSound)
			isJumping = True
			willJump = False
			If position.x <= 1
				position.x = 2
				velocity.Set(jumpForce)
			ElseIf position.x + Self.size.x + 1 >= Vsat.ScreenWidth
				position.x = Vsat.ScreenWidth - 2 - Self.size.x
				velocity.Set(-jumpForce.x, jumpForce.y)
			End
		Else
			#rem
			If velocity.x < 0 And position.x < Vsat.ScreenWidth * 0.2
				willJump = True
			ElseIf velocity.x > 0 And position.x > (Vsat.ScreenWidth * 0.8) - Self.size.x
				willJump = True
			End
			#end
			willJump = True
		End
	End


'--------------------------------------------------------------------------
' * Render
'--------------------------------------------------------------------------	
	Method Render:Void()
		If isIntroAnimating And TouchesRightWall()
			RenderIntroAnimation()
		Else
			Super.Render()
		End
		
		If Not isJumping And lastPositions.IsEmpty() = False
			lastPositions.RemoveLast()
		End
		'Version1()
		Version2()
	End
	
	Method Version1:Void()
		Local incrementAlpha:Float = (1.0 / maxPositions) * 0.2
		Local alphaCounter:Float = 0.5
		Local previous:Vec2 = position
		For Local vector:= EachIn lastPositions
			alphaCounter -= incrementAlpha
			SetAlpha(alphaCounter)
			PushMatrix()
			If previous
				DrawLineV(previous, vector)
				Translate(Self.size.x, 0)
				DrawLineV(previous, vector)
			End
			PopMatrix()
			previous = vector
		Next
	End
	
	Method Version2:Void()
		Local incrementAlpha:Float = (1.0 / maxPositions) * 0.2
		Local alphaCounter:Float = 0.3
		Local previous:Vec2 = position
		For Local vector:= EachIn lastPositions
			alphaCounter -= incrementAlpha
			SetAlpha(alphaCounter)
			PushMatrix()
			TranslateV(vector)
			DrawOutline()
			PopMatrix()
			previous = vector
		Next
	End
	
	Method DrawOutline:Void()
		DrawRectOutline(0, 0, size.x, size.y)
		DrawRectOutline(-1, -1, size.x+2, size.y+2)
	End
	
	Method RenderIntroAnimation:Void()
		color.Use()
		PushMatrix()
			Translate(position.x, position.y)
			Rotate(rotation)
			ScaleAt(size, scale.x, scale.y)
			If Not renderOutline
				Draw()
			Else
				DrawOutline()
			End
		PopMatrix()
	End
	
	
'--------------------------------------------------------------------------
' * Helper
'--------------------------------------------------------------------------
	Method TouchesLeftWall:Bool()
		Return position.x <= 1
	End
	
	Method TouchesRightWall:Bool()
		Return position.x + Self.size.x + 1 >= Vsat.ScreenWidth
	End
	
	
	Method SetLeft:Void()
		position.x = 1
	End
	
	Method SetRight:Void()
		position.x = Vsat.ScreenWidth - Self.size.x - 1
	End
	
	
	Method OnWallImpact:Void()
		
	End
	
End







