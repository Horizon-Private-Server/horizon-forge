using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using UnityEditor.Experimental.GraphView;
using UnityEngine;

public class PathGraphAgent : MonoBehaviour
{
    public Transform TargetTransform;
    public PathGraph Graph;

    public float Gravity = 30f;
    public float Radius = 0.5f;
    public float Speed = 5f;
    public float JumpSpeed = 5f;
    public float Acceleration = 5f;

    //[Range(0f, 1f)] public float PathFitFactor = 0.25f;

    private Vector3? nextPhysicsTarget = null;
    private Vector3? nextPhysicsStart = null;
    private Vector3 velocity = Vector3.zero;
    private Vector3? lastTarget = null;
    private float nextPhysicsTime = 0f;
    private List<PathGraphMemoryNode> currentPath = null;
    private List<PathGraphMemoryNode> currentFullPath = null;
    private PathGraphMemoryNode lastNodeOnPath = null;
    private bool grounded = false;

    private void OnDrawGizmos()
    {
        Vector3 closestPointOnNode;
        if (!Graph || !TargetTransform)
            return;

        if (currentPath != null && lastTarget.HasValue)
        {
            Gizmos.DrawLine(transform.position, lastTarget.Value);

            var currentPos = lastTarget.Value;
            for (int i = 1; i < currentPath.Count; ++i)
            {
                var current = currentPath[i];

                if ((currentPath.Count - i) > 1)
                {
                    closestPointOnNode = GetClosestPointOnNode(current, currentPos, currentPath[i+1]);
                }
                else
                {
                    closestPointOnNode = GetClosestPointOnNode(current, currentPos);
                }

                Gizmos.DrawLine(currentPos, closestPointOnNode);
                currentPos = closestPointOnNode;
            }

            Gizmos.DrawLine(currentPos, TargetTransform.position);
            return;
        }

        var path = Graph.FindPath(transform.position, TargetTransform.position + Vector3.up, cleanup: true);
        if (path != null)
        {
            var currentPos = transform.position;
            while (path.Count > 0)
            {
                var current = path.Pop();

                if (path.TryPeek(out var next))
                {
                    closestPointOnNode = GetClosestPointOnNode(current, currentPos, next);
                }
                else
                {
                    closestPointOnNode = GetClosestPointOnNode(current, currentPos);
                }

                //closestPointOnNode = current.Center;

                Gizmos.DrawLine(currentPos, closestPointOnNode);
                currentPos = closestPointOnNode;
            }

            Gizmos.DrawLine(currentPos, TargetTransform.position);
        }
    }

    private void FixedUpdate()
    {
        if (!TargetTransform || !Graph)
            return;

        // set current position to target from last physics update
        if (nextPhysicsTarget.HasValue)
            transform.position = nextPhysicsTarget.Value;

        // start is current position
        nextPhysicsStart = transform.position;

        // determine next transform position
        var targetPos = GetTargetPos();
        if (targetPos.x > 524)
            targetPos.x = 524;
        var direction = (targetPos - transform.position).normalized;
        var planarDirection = Vector3.ProjectOnPlane(direction, Vector3.up);

        

        var targetVelocity = planarDirection.normalized * Speed;
        if (Vector3.Distance(TargetTransform.position, transform.position) < (Radius + 1))
            targetVelocity = Vector3.zero;

        velocity += Vector3.ProjectOnPlane(targetVelocity - velocity, Vector3.up) * Time.deltaTime * Acceleration;

        if (grounded && IsOnJumpEdge())
            velocity = Vector3.up * JumpSpeed * Time.deltaTime;

        // check forward
        var nextPos = MoveCheck(transform.position, transform.position + velocity * Time.deltaTime * Speed, ref velocity);
        nextPos = MoveCheck(transform.position, nextPos, ref velocity);

        // check ground
        grounded = false;
        var isMovingDown = Vector3.Dot(velocity, Vector3.up) < 0;
        if (isMovingDown && Physics.Raycast(nextPos, Vector3.down, out var hitInfo, Radius + (Gravity * Time.deltaTime)))
        {
            grounded = true;
            nextPos = hitInfo.point + Vector3.up * Radius;
            velocity = Vector3.ProjectOnPlane(velocity, Vector3.up);
        }
        
        velocity += Vector3.down * Gravity * Time.deltaTime;
        if (velocity.y < -30f)
            velocity.y = -30f;

        transform.position = nextPos;


        nextPhysicsTarget = transform.position;
        transform.position = nextPhysicsStart.Value;
        nextPhysicsTime = 0f;
    }

    private void Update()
    {
        nextPhysicsTime += Time.deltaTime;
        float t = nextPhysicsTime / Time.fixedDeltaTime;

        if (nextPhysicsTarget.HasValue && nextPhysicsStart.HasValue)
        {
            transform.position = Vector3.Lerp(nextPhysicsStart.Value, nextPhysicsTarget.Value, t);
        }
    }
    
    private Vector3 MoveCheck(Vector3 currentPosition, Vector3 nextPosition, ref Vector3 velocity)
    {
        var posDelta = nextPosition - currentPosition;
        if (Physics.Raycast(currentPosition, posDelta, out var hitInfo, Radius + posDelta.magnitude))
        {
            // jump if hit wall
            var angle = Vector3.Angle(hitInfo.normal, Vector3.ProjectOnPlane(posDelta, Vector3.up)) - 90f;
            if (angle > 55f && grounded)
            {
                // jump
                velocity = Vector3.up * JumpSpeed * Time.deltaTime;
            }

            nextPosition = currentPosition + Vector3.Reflect(posDelta, hitInfo.normal);
        }

        return nextPosition;
    }

    private Vector3 GetTargetPos()
    {
        // determine if we need to recalculate the path
        if (ShouldFindNewPath())
            GenerateNewPath();

        if (currentPath == null || currentPath.Count == 0)
            return TargetTransform ? TargetTransform.position : transform.position;

        // remove first node if we've reached it
        if (lastTarget.HasValue && Vector3.Magnitude(Vector3.ProjectOnPlane(lastTarget.Value - transform.position, Vector3.up)) < (Radius + 0.5f) && Vector3.Distance(currentPath[0].Center, transform.position) < (currentPath[0].Radius + Radius))
        {
            lastNodeOnPath = currentPath[0];
            currentPath.RemoveAt(0);
        }
        else if (lastTarget.HasValue)
            return lastTarget.Value;

        if (currentPath == null || currentPath.Count == 0)
            return TargetTransform ? TargetTransform.position : transform.position;

        // only one node left
        if (currentPath.Count > 1)
        {
            var n1 = currentPath[0];
            var n2 = currentPath[1];
            lastTarget = GetClosestPointOnNode(n1, transform.position, n2);
        }
        else
        {
            lastTarget = GetClosestPointOnNode(currentPath[0], transform.position);
        }

        return lastTarget.Value;
    }

    private bool IsOnJumpEdge()
    {
        if (currentPath == null || currentPath.Count == 0 || lastNodeOnPath == null)
            return false;

        if (Vector3.Distance(lastNodeOnPath.Center, transform.position) < lastNodeOnPath.Radius)
            return false;

        var edge = lastNodeOnPath.Edges.FirstOrDefault(x => x.ToNode == currentPath[0]);
        return edge?.JumpPad ?? false;
    }

    private Vector3 GetClosestPointOnNode(PathGraphMemoryNode node, Vector3 from, PathGraphMemoryEdge edge)
    {
        var fit = Vector3.Lerp(from, edge.ToNode.Center, edge.PathFitStartEnd);
        var fromClosest = GetClosestPointOnNode(node, fit);

        var dot = Mathf.Abs(Vector3.Dot((node.Center - from).normalized, (edge.ToNode.Center - node.Center).normalized));
        var cornerRadius = Mathf.Lerp(node.CorneringRadius, node.Radius, Mathf.Pow(dot, 2f));
        return node.Center - Vector3.ClampMagnitude(Vector3.ProjectOnPlane(node.Center - fit, Vector3.up), cornerRadius);
    }

    private Vector3 GetClosestPointOnNode(PathGraphMemoryNode node, Vector3 from, PathGraphMemoryNode to)
    {
        var edge = node.Edges.FirstOrDefault(x => x.ToNode == to);
        if (edge != null)
            return GetClosestPointOnNode(node, from, edge);

        return GetClosestPointOnNode(node, from);
    }

    private Vector3 GetClosestPointOnNode(PathGraphMemoryNode node, Vector3 from)
    {
        var dot = Mathf.Abs(Vector3.Dot((node.Center - from).normalized, (TargetTransform.position - node.Center).normalized));
        var cornerRadius = Mathf.Lerp(node.CorneringRadius, node.Radius, Mathf.Pow(dot, 2f));
        return node.Center - Vector3.ClampMagnitude(Vector3.ProjectOnPlane(node.Center - from, Vector3.up), cornerRadius);
    }

    private void GenerateNewPath()
    {
        lastNodeOnPath = null;
        lastTarget = null;
        currentPath = Graph.FindPath(transform.position, TargetTransform.position + Vector3.up, cleanup: true).ToList();
        currentFullPath = Graph.FindPath(transform.position, TargetTransform.position + Vector3.up, cleanup: false).ToList();
    }

    private bool ShouldFindNewPath()
    {
        // path is empty
        if (currentPath == null || !lastTarget.HasValue)
            return true;

        // target is in sight
        if (currentPath.Count == 0 && Graph.InSight(transform.position, TargetTransform.position + Vector3.up))
            return false;

        var closestNodeToTarget = Graph.GetClosestNodeInSight(TargetTransform.position + Vector3.up);
        //if (lastTarget.HasValue && currentPath.Count == 0 && Vector3.Distance(closestNodeToTarget.GetCenterPosition(), lastTarget.Value) > (Radius + closestNodeToTarget.Radius))
        //    return true;

        // closest node to target has changed
        if (currentFullPath.Count > 0 && closestNodeToTarget != currentFullPath.Last().RefNode)
            return true;

        return false;
    }
}
