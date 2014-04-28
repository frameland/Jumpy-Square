Strict
Import vsat
Import particles
Import extra

Class Enemy Extends VRect
	
	Field velocity:Vec2
	Field gravity:Float = Vsat.ScreenHeight / 38
	Field link:ListNode<Enemy>
	Field widthRelative:Float = 6.4
	Field collidedWithPlayer:Bool
	Field image:Image
	
	Field lastPositions:List<Vec2> = New List<Vec2>
	Field maxPositions:Int = 5
	
'--------------------------------------------------------------------------
' * helpers for medals, etc.
'--------------------------------------------------------------------------
	Field accountsForPoints:Bool = True
	Field hasBeenScored:Bool
	Field wasClose:Bool
	Field isSurprise:Bool
	

'--------------------------------------------------------------------------
' * Init & Helpers
'--------------------------------------------------------------------------
	Method New()
		Super.New(0, -Vsat.ScreenWidth/widthRelative, Vsat.ScreenWidth/widthRelative, Vsat.ScreenWidth/widthRelative)
		color.Set(Color.Orange)
		color.Alpha = 0.0
		renderOutline = True
		velocity = New Vec2
		InitImageAndHandle()
	End
	
	Method InitImageAndHandle:Void()
		Select Vsat.ScreenHeight
			Case 960, 1136
				image = ImageCache.GetImage(RealPath("enemy.png"))
				image.SetHandle(6, 6)
			Case 1024
				
			Case 2048
				
		End
	End
	
	Method SetLeft:Void()
		position.x = 1
	End
	
	Method SetRight:Void()
		position.x = Vsat.ScreenWidth - Self.size.x - 1
	End
	
	Method Remove:Void()
		If link Then link.Remove()
	End
	
	
'--------------------------------------------------------------------------
' * Update & Render
'--------------------------------------------------------------------------	
	Method UpdatePhysics:Void(dt:Float)
		UpdateLastPosition()
		
		If color.Alpha < 1
			color.Alpha += 2 * dt
		End
		
		velocity.y += gravity
		velocity.Limit(gravity * 50)
		position.Add(velocity.x * dt, velocity.y * dt)
		If position.y > Vsat.ScreenHeight
			Self.Remove()
			If accountsForPoints
				Local ev:= New VEvent
				ev.id = "RemoveEnemy"
				Vsat.FireEvent(ev)
			End
		End
	End
	
	Method UpdateLastPosition:Void()
		lastPositions.AddFirst(New Vec2(position))
		If lastPositions.Count() > maxPositions
			lastPositions.RemoveLast()
		End
	End
	
	Method Render:Void()
		Super.Render()
		
		Local incrementAlpha:Float = (1.0 / maxPositions) * 0.2
		Local alphaCounter:Float = 0.2
		Local previous:Vec2 = position
		For Local vector:= EachIn lastPositions
			alphaCounter -= incrementAlpha
			Local multiplier:Float = alphaCounter * Self.color.Alpha
			SetAlpha(multiplier)
			PushMatrix()
			TranslateV(vector)
			DrawOutline()
			PopMatrix()
			previous = vector
			
		Next
	End
	
	
	Method DrawOutline:Void()
		DrawImage(image, 0, 0)
	End

	
End
